#include "emporiavue.h"
#include "external_samd_firmware.h"
#include "samd09_firmware.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome {
namespace emporiavue {

void EmporiaVueComponent::backup_firmware() {
  if (this->install_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware install is running; firmware backup ignored");
    return;
  }
  if (this->backup_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware backup is already running");
    return;
  }

  this->last_error_.clear();
  this->backup_partition_ = nullptr;
  this->backup_core_halted_ = false;
  this->backup_header_written_ = false;
  this->backup_partition_finalized_ = false;
  this->backup_log_only_ = false;
  this->backup_stage_ = BackupStage::IDLE;
  this->backup_next_offset_ = 0;
  this->backup_flash_size_ = 0;
  std::fill(this->backup_stored_hash_.begin(), this->backup_stored_hash_.end(), 0);
  std::fill(this->backup_verify_hash_.begin(), this->backup_verify_hash_.end(), 0);

  ESP_LOGI(TAG, "Starting SAMD09 legacy firmware backup");

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    return;
  }

  this->backup_log_only_ = !this->find_backup_partition_();
  if (this->backup_log_only_) {
    ESP_LOGW(TAG, "SAMD backup partition missing; firmware will be dumped to the log instead");
  }

  this->stop_i2c_diagnostics_();
  this->stop_metering_();
  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    this->release_pins_();
    this->start_i2c_diagnostics_();
    this->start_metering_();
    ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    this->release_pins_();
    this->start_i2c_diagnostics_();
    this->start_metering_();
    ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    this->release_pins_();
    this->start_i2c_diagnostics_();
    this->start_metering_();
    ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->halt_core_()) {
    this->release_pins_();
    this->start_i2c_diagnostics_();
    this->start_metering_();
    ESP_LOGW(TAG, "SAMD09 firmware backup failed while halting core: %s", this->last_error_.c_str());
    return;
  }
  this->backup_core_halted_ = true;

  if (!this->read_flash_geometry_(&this->backup_nvm_param_, &this->backup_page_size_, &this->backup_page_count_,
                                  &this->backup_flash_size_)) {
    this->fail_backup_("geometry read failed: " + this->last_error_);
    return;
  }
  if (this->backup_flash_size_ == 0 || this->backup_flash_size_ > 0x20000UL) {
    this->fail_backup_(str_sprintf("unsupported SAMD flash size %" PRIu32, this->backup_flash_size_));
    return;
  }

  if (!this->mem_read32_(DSU_DID, &this->backup_dsu_did_)) {
    this->backup_dsu_did_ = 0;
    ESP_LOGW(TAG, "SAMD09 backup could not read DSU DID: %s", this->last_error_.c_str());
    this->last_error_.clear();
  }

  bool managed = false;
  if (!this->detect_managed_firmware_(this->backup_flash_size_, &managed)) {
    this->fail_backup_("firmware detection failed: " + this->last_error_);
    return;
  }
  if (managed) {
    this->fail_backup_("managed firmware detected; refusing to back up managed SAMD firmware");
    return;
  }

  if (!this->backup_log_only_) {
    if (!this->backup_partition_has_capacity_(this->backup_flash_size_)) {
      this->fail_backup_("backup partition too small");
      return;
    }
    ESP_LOGI(TAG, "Erasing SAMD backup partition '%s' (%" PRIu32 " bytes)", this->backup_partition_name_.c_str(),
             static_cast<uint32_t>(this->backup_partition_->size));
    esp_err_t err = esp_partition_erase_range(this->backup_partition_, 0, this->backup_partition_->size);
    if (err != ESP_OK) {
      this->fail_backup_(str_sprintf("backup partition erase failed: 0x%X", static_cast<unsigned>(err)));
      return;
    }

    if (!this->write_backup_header_(BACKUP_STATE_IN_PROGRESS, this->backup_flash_size_, this->backup_page_size_,
                                    this->backup_page_count_, this->backup_nvm_param_, this->backup_dsu_did_)) {
      this->fail_backup_("backup header write failed");
      return;
    }
    this->backup_header_written_ = true;
  }

  this->backup_next_offset_ = 0;
  if (this->backup_log_only_) {
    mbedtls_sha256_init(&this->backup_sha_ctx_);
    mbedtls_sha256_starts(&this->backup_sha_ctx_, 0);
    this->backup_sha_ctx_active_ = true;
    this->backup_stage_ = BackupStage::READ_AND_LOG;
  } else {
    this->backup_stage_ = BackupStage::READ_AND_STORE;
  }
  this->backup_active_ = true;
  ESP_LOGI(TAG,
           "SAMD09 legacy firmware backup started: flash_size=%" PRIu32 ", page_size=%" PRIu32
           ", page_count=%" PRIu32 ", destination=%s",
           this->backup_flash_size_, this->backup_page_size_, this->backup_page_count_,
           this->backup_log_only_ ? "log" : "partition+log");
}

void EmporiaVueComponent::install_firmware() { this->start_firmware_action_(FirmwareAction::UPDATE_MANAGED, true); }

void EmporiaVueComponent::restore_firmware() { this->start_firmware_action_(FirmwareAction::RESTORE_STOCK, false); }

void EmporiaVueComponent::flash_external_firmware(uint8_t index) {
  this->start_firmware_action_(FirmwareAction::FLASH_EXTERNAL, true, index);
}

void EmporiaVueComponent::start_firmware_action_(FirmwareAction requested_action, bool force_update,
                                                uint8_t external_firmware_index) {
  if (requested_action != FirmwareAction::UPDATE_MANAGED && requested_action != FirmwareAction::RESTORE_STOCK &&
      requested_action != FirmwareAction::FLASH_EXTERNAL) {
    ESP_LOGW(TAG, "Unsupported SAMD09 firmware action requested");
    return;
  }
  const bool restore_requested = requested_action == FirmwareAction::RESTORE_STOCK;
  const bool external_requested = requested_action == FirmwareAction::FLASH_EXTERNAL;
  const char *requested_name =
      restore_requested ? "backup flash" : (external_requested ? "external flash" : "bundled flash");

  if (this->install_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware %s is already running", this->install_action_name_());
    return;
  }
  if (this->backup_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware backup is running; firmware %s ignored", requested_name);
    return;
  }

  this->last_error_.clear();
  this->install_core_halted_ = false;
  this->install_started_writing_ = false;
  this->install_action_ = FirmwareAction::UNKNOWN;
  this->install_source_ = FlashSource::NONE;
  this->install_backup_header_ = BackupHeader{};
  this->install_stage_ = InstallStage::IDLE;
  this->install_next_offset_ = 0;
  this->install_next_progress_log_offset_ = 0;
  this->install_flash_size_ = 0;
  this->install_page_size_ = 0;
  this->install_row_size_ = 0;
  this->install_external_firmware_index_ = external_firmware_index;
  ESP_LOGD(TAG, "Starting SAMD09 firmware %s check", requested_name);

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    return;
  }

  this->stop_i2c_diagnostics_();
  this->stop_metering_();
  bool core_halted = false;
  auto release_after_check = [&]() {
    if (core_halted) {
      const std::string prior_error = this->last_error_;
      if (!this->resume_core_()) {
        ESP_LOGW(TAG, "Failed to resume SAMD09 core after firmware update check: %s", this->last_error_.c_str());
        this->last_error_ = prior_error;
      }
      core_halted = false;
    }
    this->release_pins_();
    this->start_i2c_diagnostics_();
    this->start_metering_();
  };

  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware update check failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware update check failed: %s", this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware update check failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->halt_core_()) {
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware update failed while halting core: %s", this->last_error_.c_str());
    return;
  }
  core_halted = true;

  FirmwareInfo current{};
  if (external_requested) {
    if (!this->read_flash_geometry_(&current.nvm_param, &current.page_size, &current.page_count,
                                    &current.flash_size)) {
      release_after_check();
      ESP_LOGW(TAG, "SAMD09 external firmware flash failed reading flash geometry: %s", this->last_error_.c_str());
      return;
    }
    current.kind = FirmwareKind::UNKNOWN;
    current.source = FirmwareDetectionSource::SWD;
  } else {
    if (!this->read_current_firmware_info_(&current)) {
      release_after_check();
      ESP_LOGW(TAG, "SAMD09 firmware update check failed reading current firmware info: %s", this->last_error_.c_str());
      return;
    }
    this->detected_firmware_info_ = current;
    this->detected_firmware_info_valid_ = true;
    this->publish_firmware_version_(current);
  }

  BackupHeader backup_header{};
  std::string backup_error;
  std::string action_reason;
  FirmwareAction selected_action = FirmwareAction::UNKNOWN;

  if (restore_requested) {
    if (!this->read_valid_backup_(&backup_header, &backup_error)) {
      release_after_check();
      ESP_LOGW(TAG, "SAMD09 firmware restore blocked: valid backup required (%s)", backup_error.c_str());
      return;
    }
    selected_action = FirmwareAction::RESTORE_STOCK;
    action_reason = "valid backup is available";
  } else if (external_requested) {
    if (!this->external_firmware_available_(external_firmware_index)) {
      release_after_check();
      ESP_LOGW(TAG, "SAMD09 external firmware flash requested but no external image is compiled in");
      return;
    }
    selected_action = FirmwareAction::FLASH_EXTERNAL;
    action_reason = "manual external firmware requested";
  } else {
    if (force_update) {
      selected_action = FirmwareAction::UPDATE_MANAGED;
      action_reason = "manual update requested";
    } else {
      selected_action = this->determine_firmware_action_(current, &action_reason);
      if (selected_action == FirmwareAction::UNKNOWN) {
        release_after_check();
        ESP_LOGW(TAG, "SAMD09 firmware update blocked: %s", action_reason.c_str());
        return;
      }
      if (selected_action != FirmwareAction::UPDATE_MANAGED) {
        release_after_check();
        ESP_LOGI(TAG, "SAMD09 firmware update not needed: %s", action_reason.c_str());
        return;
      }
    }

    if (!this->bundled_firmware_available_()) {
      release_after_check();
      ESP_LOGW(TAG, "SAMD09 managed firmware update requested but no bundled firmware image is compiled in");
      return;
    }

    if (!this->bundled_firmware_matches_target_()) {
      release_after_check();
      ESP_LOGW(TAG, "SAMD09 firmware update blocked by bundled image target mismatch");
      return;
    }
  }

  uint32_t source_size = this->bundled_firmware_size_();
  if (selected_action == FirmwareAction::RESTORE_STOCK) {
    source_size = backup_header.flash_size;
  } else if (selected_action == FirmwareAction::FLASH_EXTERNAL) {
    source_size = this->external_firmware_size_(external_firmware_index);
  }
  if (selected_action == FirmwareAction::FLASH_EXTERNAL && source_size > current.flash_size) {
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware %s blocked because external image is larger than flash", requested_name);
    return;
  }
  if (selected_action != FirmwareAction::FLASH_EXTERNAL && source_size != current.flash_size) {
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware %s blocked by image/flash size mismatch", requested_name);
    return;
  }

  if (current.page_size == 0 || (current.page_size % 4U) != 0 || current.flash_size == 0 ||
      (current.flash_size % current.page_size) != 0 || current.page_size > BACKUP_IO_BLOCK_SIZE) {
    release_after_check();
    ESP_LOGW(TAG, "SAMD09 firmware %s blocked by unsupported flash geometry", requested_name);
    return;
  }

  auto firmware_label = [this](const FirmwareInfo &info) -> std::string {
    switch (info.kind) {
      case FirmwareKind::MANAGED:
        return str_sprintf("v%s (%s)", format_firmware_version_(info.version).c_str(),
                           firmware_mode_name_(info.mode_id));
      case FirmwareKind::STOCK:
        return "stock";
      case FirmwareKind::UNKNOWN:
      default:
        return "unknown";
    }
  };
  std::string target_label;
  if (selected_action == FirmwareAction::UPDATE_MANAGED) {
    target_label = str_sprintf("v%s (%s)", format_firmware_version_(this->bundled_firmware_version_()).c_str(),
                               firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())));
  } else if (selected_action == FirmwareAction::RESTORE_STOCK) {
    target_label = "backup firmware";
  } else {
    target_label = "external firmware";
  }

  this->install_core_halted_ = core_halted;
  core_halted = false;

  if (!this->nvm_clear_errors_()) {
    this->fail_install_("NVM error clear failed: " + this->last_error_);
    return;
  }
  if (!this->mem_write32_(NVMCTRL_CTRLB, 0x00000082UL)) {
    this->fail_install_("NVM manual write setup failed: " + this->last_error_);
    return;
  }
  if (!this->nvm_wait_ready_()) {
    this->fail_install_("NVM not ready: " + this->last_error_);
    return;
  }

  this->install_action_ = selected_action;
  if (selected_action == FirmwareAction::RESTORE_STOCK) {
    this->install_source_ = FlashSource::BACKUP;
  } else if (selected_action == FirmwareAction::FLASH_EXTERNAL) {
    this->install_source_ = FlashSource::EXTERNAL;
  } else {
    this->install_source_ = FlashSource::BUNDLED;
  }
  if (selected_action == FirmwareAction::RESTORE_STOCK) {
    this->install_backup_header_ = backup_header;
  }
  this->install_flash_size_ = current.flash_size;
  this->install_page_size_ = current.page_size;
  this->install_row_size_ = current.page_size * NVM_PAGES_PER_ROW;
  this->install_next_offset_ = 0;
  this->install_next_progress_log_offset_ = INSTALL_PROGRESS_LOG_INTERVAL;
  this->install_stage_ = InstallStage::FLASH_PAGES;
  this->install_active_ = true;
  ESP_LOGI(TAG, "SAMD09 firmware %s started: %s -> %s, %" PRIu32 " bytes, %s", requested_name,
           firmware_label(current).c_str(), target_label.c_str(), source_size, action_reason.c_str());
}

void EmporiaVueComponent::inspect_backup_partition_() {
  if (!this->find_backup_partition_()) {
    return;
  }

  BackupHeader header{};
  if (!this->read_backup_header_(&header) || header.magic != BACKUP_MAGIC) {
    return;
  }

  if (header.state == BACKUP_STATE_IN_PROGRESS) {
    return;
  }
  if (header.state == BACKUP_STATE_INVALID) {
    return;
  }
  if (header.state != BACKUP_STATE_VALID) {
    return;
  }
  if (header.version != BACKUP_HEADER_VERSION || header.header_size != sizeof(BackupHeader) ||
      !this->backup_partition_has_capacity_(header.flash_size)) {
    return;
  }

  BackupFooter footer{};
  const uint32_t footer_offset = header.image_offset + header.flash_size;
  if (esp_partition_read(this->backup_partition_, footer_offset, &footer, sizeof(footer)) != ESP_OK ||
      footer.magic != BACKUP_FOOTER_MAGIC || footer.flash_size != header.flash_size ||
      std::memcmp(footer.sha256, header.sha256, sizeof(header.sha256)) != 0) {
    return;
  }

  uint8_t hash[32]{};
  if (!this->hash_partition_image_(header.flash_size, hash)) {
    return;
  }
  if (std::memcmp(hash, header.sha256, sizeof(hash)) != 0) {
    return;
  }
}

bool EmporiaVueComponent::find_backup_partition_() {
  this->backup_partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                                     this->backup_partition_name_.c_str());
  if (this->backup_partition_ == nullptr) {
    ESP_LOGW(TAG, "SAMD backup partition '%s' not found", this->backup_partition_name_.c_str());
    return false;
  }
  return true;
}

bool EmporiaVueComponent::backup_partition_has_capacity_(uint32_t flash_size) {
  if (this->backup_partition_ == nullptr) {
    return false;
  }
  const uint32_t required = BACKUP_IMAGE_OFFSET + flash_size + sizeof(BackupFooter);
  return this->backup_partition_->size >= required;
}

bool EmporiaVueComponent::read_backup_header_(BackupHeader *header) {
  if (this->backup_partition_ == nullptr) {
    return false;
  }
  return esp_partition_read(this->backup_partition_, 0, header, sizeof(*header)) == ESP_OK;
}

bool EmporiaVueComponent::hash_partition_image_(uint32_t flash_size, uint8_t hash[32]) {
  if (this->backup_partition_ == nullptr || !this->backup_partition_has_capacity_(flash_size)) {
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);

  uint8_t buffer[256];
  uint32_t offset = 0;
  while (offset < flash_size) {
    const uint32_t length = std::min<uint32_t>(sizeof(buffer), flash_size - offset);
    if (esp_partition_read(this->backup_partition_, BACKUP_IMAGE_OFFSET + offset, buffer, length) != ESP_OK) {
      mbedtls_sha256_free(&ctx);
      return false;
    }
    mbedtls_sha256_update(&ctx, buffer, length);
    offset += length;
  }

  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  return true;
}

bool EmporiaVueComponent::write_backup_header_(uint8_t state, uint32_t flash_size, uint32_t page_size,
                                               uint32_t page_count, uint32_t nvm_param, uint32_t dsu_did) {
  if (this->backup_partition_ == nullptr) {
    return false;
  }

  BackupHeader header{};
  header.magic = BACKUP_MAGIC;
  header.version = BACKUP_HEADER_VERSION;
  header.state = state;
  header.header_size = sizeof(BackupHeader);
  header.image_offset = BACKUP_IMAGE_OFFSET;
  header.flash_size = flash_size;
  header.page_size = page_size;
  header.page_count = page_count;
  header.nvm_param = nvm_param;
  header.dsu_did = dsu_did;
  std::memset(header.sha256, 0xFF, sizeof(header.sha256));
  std::memset(header.reserved, 0xFF, sizeof(header.reserved));

  return esp_partition_write(this->backup_partition_, 0, &header, sizeof(header)) == ESP_OK;
}

bool EmporiaVueComponent::write_backup_state_(uint8_t state) {
  if (this->backup_partition_ == nullptr) {
    return false;
  }
  const uint8_t state_word[4] = {
      static_cast<uint8_t>(BACKUP_HEADER_VERSION & 0xFFU),
      static_cast<uint8_t>((BACKUP_HEADER_VERSION >> 8) & 0xFFU),
      state,
      0x00,
  };
  return esp_partition_write(this->backup_partition_, offsetof(BackupHeader, version), state_word,
                             sizeof(state_word)) == ESP_OK;
}

bool EmporiaVueComponent::write_backup_hash_and_footer_(const uint8_t hash[32], uint32_t flash_size) {
  if (this->backup_partition_ == nullptr || !this->backup_partition_has_capacity_(flash_size)) {
    return false;
  }
  if (esp_partition_write(this->backup_partition_, offsetof(BackupHeader, sha256), hash, 32) != ESP_OK) {
    return false;
  }

  BackupFooter footer{};
  footer.magic = BACKUP_FOOTER_MAGIC;
  footer.flash_size = flash_size;
  std::memcpy(footer.sha256, hash, sizeof(footer.sha256));
  return esp_partition_write(this->backup_partition_, BACKUP_IMAGE_OFFSET + flash_size, &footer, sizeof(footer)) ==
         ESP_OK;
}

bool EmporiaVueComponent::firmware_mode_matches_runtime_() const {
  if (!this->detected_firmware_info_valid_) {
    return true;
  }
  if (this->runtime_mode_ == RuntimeMode::SPI && this->detected_firmware_info_.kind != FirmwareKind::MANAGED) {
    return false;
  }
  return this->detected_firmware_info_.kind != FirmwareKind::MANAGED ||
         this->detected_firmware_info_.mode_id == this->expected_firmware_mode_id_();
}

void EmporiaVueComponent::publish_firmware_mode_mismatch_() {
  const char *detected_mode = this->detected_firmware_info_.kind == FirmwareKind::MANAGED
                                  ? firmware_mode_name_(this->detected_firmware_info_.mode_id)
                                  : "stock/unknown";
  const char *configured_mode = firmware_mode_name_(this->expected_firmware_mode_id_());
  ESP_LOGD(TAG, "Skipping SAMD runtime reads: firmware mode is %s but configured mode is %s; update SAMD firmware",
           detected_mode, configured_mode);
}

void EmporiaVueComponent::start_firmware_mode_mismatch_log_() {
  if (this->firmware_mode_mismatch_log_started_) {
    return;
  }
  this->publish_firmware_mode_mismatch_();
  this->firmware_mode_mismatch_log_started_ = true;
  this->set_interval("samd_firmware_mode_mismatch", 5000, [this]() {
    if (this->backup_active_ || this->install_active_) {
      return;
    }
    if (this->firmware_mode_matches_runtime_()) {
      this->stop_firmware_mode_mismatch_log_();
      this->start_i2c_diagnostics_();
      this->start_metering_();
      this->setup_spi_receiver_();
      return;
    }
    this->publish_firmware_mode_mismatch_();
  });
}

void EmporiaVueComponent::stop_firmware_mode_mismatch_log_() {
  if (!this->firmware_mode_mismatch_log_started_) {
    return;
  }
  this->cancel_interval("samd_firmware_mode_mismatch");
  this->firmware_mode_mismatch_log_started_ = false;
}

void EmporiaVueComponent::probe_runtime_i2c_after_firmware_update_() {
  if (this->runtime_mode_ != RuntimeMode::I2C) {
    return;
  }

  ManagedI2CDiagnostic diagnostic{};
  const ManagedI2CDiagnosticResult info_result = this->query_managed_i2c_diagnostic_(&diagnostic);
  if (info_result == ManagedI2CDiagnosticResult::VALID_RESPONSE) {
    this->publish_firmware_info_from_diagnostic_(diagnostic);
    this->publish_i2c_diagnostics_(diagnostic);
    ESP_LOGI(TAG, "SAMD09 runtime I2C diagnostic OK after update");
  } else {
    ESP_LOGW(TAG, "SAMD09 runtime I2C diagnostic failed after update: result=%u",
             static_cast<unsigned>(info_result));
  }
  this->start_i2c_diagnostics_();
  this->start_metering_();
}

void EmporiaVueComponent::publish_initial_firmware_detection_() {
  if (this->backup_active_ || this->install_active_) {
    return;
  }

  FirmwareInfo info{};
  std::string error;
  if (!this->detect_current_firmware_by_swd_(&info, &error)) {
    FirmwareInfo unknown_info{};
    this->detected_firmware_info_ = unknown_info;
    this->detected_firmware_info_valid_ = false;
    this->publish_firmware_version_(unknown_info);
    return;
  }

  this->detected_firmware_info_ = info;
  this->detected_firmware_info_valid_ = true;
  this->publish_firmware_version_(info);

  if (!this->auto_update_samd_) {
    return;
  }

  std::string auto_update_reason;
  if (!this->should_auto_update_samd_(info, &auto_update_reason)) {
    ESP_LOGI(TAG, "SAMD09 auto-update not needed: %s", auto_update_reason.c_str());
    return;
  }

  ESP_LOGI(TAG, "SAMD09 auto-update starting: %s", auto_update_reason.c_str());
  this->start_firmware_action_(FirmwareAction::UPDATE_MANAGED, false);
}

bool EmporiaVueComponent::detect_current_firmware_by_swd_(FirmwareInfo *info, std::string *error) {
  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    if (error != nullptr) {
      *error = "SWD pins missing";
    }
    return false;
  }

  auto fail = [&](const std::string &message) {
    if (error != nullptr) {
      *error = message;
    }
    ESP_LOGW(TAG, "SAMD09 SWD firmware detection failed: %s", message.c_str());
    return false;
  };

  this->last_error_.clear();
  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    const std::string message = this->last_error_.empty() ? "SWD initialization failed" : this->last_error_;
    this->finish_swd_session_();
    this->release_pins_();
    return fail(message);
  }

  if (!this->power_up_debug_()) {
    const std::string message = this->last_error_.empty() ? "debug power-up failed" : this->last_error_;
    this->finish_swd_session_();
    this->release_pins_();
    return fail(message);
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    const std::string message = this->last_error_.empty() ? "MEM-AP verification failed" : this->last_error_;
    this->release_pins_();
    return fail(message);
  }

  if (!this->read_current_firmware_info_(info)) {
    const std::string message = this->last_error_.empty() ? "firmware footer read failed" : this->last_error_;
    this->release_pins_();
    return fail(message);
  }

  if (this->reset_pin_ != nullptr) {
    this->assert_reset_();
    this->deassert_reset_();
  } else {
    const std::string prior_error = this->last_error_;
    if (!this->system_reset_core_()) {
      ESP_LOGW(TAG, "Failed to reset SAMD09 core after SWD firmware detection: %s", this->last_error_.c_str());
      this->last_error_ = prior_error;
    }
  }
  this->release_pins_();
  ESP_LOGI(TAG, "SAMD09 SWD firmware detection complete: %s",
           info->kind == FirmwareKind::MANAGED ? "managed footer found" : "no managed footer");
  return true;
}

bool EmporiaVueComponent::should_auto_update_samd_(const FirmwareInfo &info, std::string *reason) const {
  auto set_reason = [&](const std::string &message) {
    if (reason != nullptr) {
      *reason = message;
    }
  };

  if (!this->bundled_firmware_available_()) {
    set_reason("no bundled firmware image");
    return false;
  }
  if (!this->bundled_firmware_matches_target_()) {
    set_reason(str_sprintf("bundled firmware hw=%" PRIu32 " %s does not match configured hw=%u %s",
                           this->bundled_firmware_hardware_id_(),
                           firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                           static_cast<unsigned>(this->hardware_id_),
                           firmware_mode_name_(this->expected_firmware_mode_id_())));
    return false;
  }

  if (info.kind == FirmwareKind::STOCK) {
    set_reason(str_sprintf("stock -> managed hw=%" PRIu32 " %s v%s", this->bundled_firmware_hardware_id_(),
                           firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                           format_firmware_version_(this->bundled_firmware_version_()).c_str()));
    return true;
  }

  if (info.kind != FirmwareKind::MANAGED) {
    set_reason("firmware state is unknown");
    return false;
  }

  if (this->hardware_id_ != 0 && info.hardware_id != this->hardware_id_) {
    set_reason(str_sprintf("managed hw=%u does not match configured hw=%u",
                           static_cast<unsigned>(info.hardware_id), static_cast<unsigned>(this->hardware_id_)));
    return false;
  }

  if (info.mode_id != this->expected_firmware_mode_id_()) {
    set_reason(str_sprintf("managed mode %s -> %s", firmware_mode_name_(info.mode_id),
                           firmware_mode_name_(this->expected_firmware_mode_id_())));
    return this->bundled_firmware_matches_target_();
  }

  if (info.version < this->bundled_firmware_version_()) {
    set_reason(str_sprintf("managed v%s -> v%s", format_firmware_version_(info.version).c_str(),
                           format_firmware_version_(this->bundled_firmware_version_()).c_str()));
    return true;
  }

  set_reason(str_sprintf("managed hw=%u %s v%s is not older than bundled v%s",
                         static_cast<unsigned>(info.hardware_id), firmware_mode_name_(info.mode_id),
                         format_firmware_version_(info.version).c_str(),
                         format_firmware_version_(this->bundled_firmware_version_()).c_str()));
  return false;
}

bool EmporiaVueComponent::detect_managed_firmware_(uint32_t flash_size, bool *managed) {
  ManagedFirmwareInfo managed_info{};
  bool found = false;
  if (!this->read_managed_firmware_info_(flash_size, &managed_info, &found)) {
    return false;
  }
  *managed = found;
  return true;
}

bool EmporiaVueComponent::read_managed_firmware_info_(uint32_t flash_size, ManagedFirmwareInfo *managed_info,
                                                      bool *found) {
  *found = false;
  if (flash_size < sizeof(ManagedFirmwareInfo)) {
    return true;
  }

  uint8_t raw_info[sizeof(ManagedFirmwareInfo)]{};
  const uint32_t info_address = FLASH_START + flash_size - sizeof(ManagedFirmwareInfo);
  if (!this->read_flash_bytes_(info_address, sizeof(raw_info), raw_info)) {
    return false;
  }

  ManagedFirmwareInfo candidate{};
  std::memcpy(&candidate, raw_info, sizeof(candidate));
  if (std::memcmp(candidate.marker, MANAGED_MARKER, MANAGED_MARKER_LENGTH) == 0 &&
      candidate.firmware_version != 0 &&
      (candidate.mode_id == MANAGED_MODE_I2C || candidate.mode_id == MANAGED_MODE_SPI)) {
    *managed_info = candidate;
    *found = true;
    return true;
  }

  LegacyMagicManagedFirmwareInfo legacy_magic{};
  std::memcpy(&legacy_magic, raw_info, sizeof(legacy_magic));
  if (legacy_magic.magic == LEGACY_MANAGED_INFO_MAGIC &&
      std::memcmp(legacy_magic.marker, MANAGED_MARKER, MANAGED_MARKER_LENGTH) == 0) {
    managed_info->hardware_id = legacy_magic.hardware_id;
    managed_info->mode_id = MANAGED_MODE_I2C;
    managed_info->firmware_version = legacy_magic.firmware_version;
    std::memcpy(managed_info->image_sha256, legacy_magic.image_sha256, sizeof(managed_info->image_sha256));
    std::memcpy(managed_info->marker, legacy_magic.marker, sizeof(managed_info->marker));
    std::memset(managed_info->reserved, 0xFF, sizeof(managed_info->reserved));
    *found = true;
    return true;
  }

  LegacyManagedFirmwareInfo legacy{};
  std::memcpy(&legacy, raw_info, sizeof(legacy));
  if (legacy.magic == LEGACY_MANAGED_INFO_MAGIC && legacy.format_version == MANAGED_INFO_FORMAT_VERSION &&
      std::memcmp(legacy.marker, MANAGED_MARKER, MANAGED_MARKER_LENGTH) == 0) {
    managed_info->hardware_id = legacy.hardware_id;
    managed_info->mode_id = MANAGED_MODE_I2C;
    managed_info->firmware_version = legacy.firmware_version;
    std::memcpy(managed_info->image_sha256, legacy.image_sha256, sizeof(managed_info->image_sha256));
    std::memcpy(managed_info->marker, legacy.marker, sizeof(managed_info->marker));
    std::memset(managed_info->reserved, 0xFF, sizeof(managed_info->reserved));
    *found = true;
    return true;
  }

  return true;
}

bool EmporiaVueComponent::read_current_firmware_info_(FirmwareInfo *info) {
  uint32_t nvm_param = 0;
  uint32_t page_size = 0;
  uint32_t page_count = 0;
  uint32_t flash_size = 0;
  if (!this->read_flash_geometry_(&nvm_param, &page_size, &page_count, &flash_size)) {
    return false;
  }

  info->kind = FirmwareKind::STOCK;
  info->hardware_id = 0;
  info->version = 0;
  info->flash_size = flash_size;
  info->image_size = flash_size;
  info->i2c_frame_length = STOCK_I2C_FRAME_SIZE;
  info->source = FirmwareDetectionSource::SWD;
  info->page_size = page_size;
  info->page_count = page_count;
  info->nvm_param = nvm_param;

  ManagedFirmwareInfo managed_info{};
  bool managed_found = false;
  if (!this->read_managed_firmware_info_(flash_size, &managed_info, &managed_found)) {
    return false;
  }

  if (managed_found) {
    info->kind = FirmwareKind::MANAGED;
    info->hardware_id = managed_info.hardware_id;
    info->mode_id = managed_info.mode_id;
    info->version = managed_info.firmware_version;
    std::memcpy(info->image_sha256, managed_info.image_sha256, sizeof(info->image_sha256));
  }

  return true;
}

bool EmporiaVueComponent::read_valid_backup_(BackupHeader *header, std::string *error) {
  if (!this->find_backup_partition_()) {
    if (error != nullptr) {
      *error = "partition missing";
    }
    return false;
  }

  BackupHeader candidate{};
  if (!this->read_backup_header_(&candidate) || candidate.magic != BACKUP_MAGIC) {
    if (error != nullptr) {
      *error = "backup missing";
    }
    return false;
  }
  if (candidate.state != BACKUP_STATE_VALID) {
    if (error != nullptr) {
      *error = "backup not valid";
    }
    return false;
  }
  if (candidate.version != BACKUP_HEADER_VERSION || candidate.header_size != sizeof(BackupHeader) ||
      candidate.image_offset != BACKUP_IMAGE_OFFSET || candidate.flash_size == 0 || candidate.page_size == 0 ||
      !this->backup_partition_has_capacity_(candidate.flash_size)) {
    if (error != nullptr) {
      *error = "backup metadata invalid";
    }
    return false;
  }

  BackupFooter footer{};
  const uint32_t footer_offset = candidate.image_offset + candidate.flash_size;
  if (esp_partition_read(this->backup_partition_, footer_offset, &footer, sizeof(footer)) != ESP_OK ||
      footer.magic != BACKUP_FOOTER_MAGIC || footer.flash_size != candidate.flash_size ||
      std::memcmp(footer.sha256, candidate.sha256, sizeof(candidate.sha256)) != 0) {
    if (error != nullptr) {
      *error = "backup footer invalid";
    }
    return false;
  }

  uint8_t hash[32]{};
  if (!this->hash_partition_image_(candidate.flash_size, hash)) {
    if (error != nullptr) {
      *error = "backup hash read failed";
    }
    return false;
  }
  if (std::memcmp(hash, candidate.sha256, sizeof(hash)) != 0) {
    if (error != nullptr) {
      *error = "backup hash mismatch";
    }
    return false;
  }

  if (header != nullptr) {
    *header = candidate;
  }
  return true;
}

EmporiaVueComponent::FirmwareAction EmporiaVueComponent::determine_firmware_action_(
    const FirmwareInfo &current, std::string *reason) const {
  if (current.kind == FirmwareKind::STOCK) {
    if (!this->bundled_firmware_available_()) {
      if (reason != nullptr) {
        *reason = "stock firmware detected; no bundled managed firmware image is compiled in";
      }
      return FirmwareAction::NONE;
    }
    if (!this->bundled_firmware_matches_target_()) {
      if (reason != nullptr) {
        *reason = str_sprintf("stock firmware detected; no bundled managed firmware for configured hw=%u %s",
                              static_cast<unsigned>(this->hardware_id_),
                              firmware_mode_name_(this->expected_firmware_mode_id_()));
      }
      return FirmwareAction::NONE;
    }
    if (reason != nullptr) {
      *reason = str_sprintf("stock firmware detected; update to managed hw=%" PRIu32 " %s v%s",
                            this->bundled_firmware_hardware_id_(),
                            firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                            format_firmware_version_(this->bundled_firmware_version_()).c_str());
    }
    return FirmwareAction::UPDATE_MANAGED;
  }

  if (current.kind == FirmwareKind::MANAGED && this->hardware_id_ != 0 &&
      current.hardware_id != this->hardware_id_) {
    if (reason != nullptr) {
      if (this->bundled_firmware_matches_target_()) {
        *reason = str_sprintf("managed hardware id %u does not match configured hardware id %u",
                              static_cast<unsigned>(current.hardware_id), static_cast<unsigned>(this->hardware_id_));
      } else {
        *reason = str_sprintf("managed hardware id %u does not match configured hardware id %u; "
                              "no compatible bundled managed firmware is available",
                              static_cast<unsigned>(current.hardware_id), static_cast<unsigned>(this->hardware_id_));
      }
    }
    return this->bundled_firmware_matches_target_() ? FirmwareAction::UPDATE_MANAGED : FirmwareAction::NONE;
  }

  if (current.kind == FirmwareKind::MANAGED &&
      current.mode_id != this->expected_firmware_mode_id_()) {
    if (reason != nullptr) {
      *reason = str_sprintf("managed mode %s does not match configured mode %s",
                            firmware_mode_name_(current.mode_id),
                            firmware_mode_name_(this->expected_firmware_mode_id_()));
    }
    return this->bundled_firmware_matches_target_() ? FirmwareAction::UPDATE_MANAGED : FirmwareAction::NONE;
  }

  if (current.kind == FirmwareKind::MANAGED && this->bundled_firmware_matches_target_() &&
      current.version < this->bundled_firmware_version_()) {
    if (reason != nullptr) {
      *reason = str_sprintf("managed hw=%u %s v%s is older than bundled v%s",
                            static_cast<unsigned>(current.hardware_id), firmware_mode_name_(current.mode_id),
                            format_firmware_version_(current.version).c_str(),
                            format_firmware_version_(this->bundled_firmware_version_()).c_str());
    }
    return FirmwareAction::UPDATE_MANAGED;
  }

  if (current.kind == FirmwareKind::MANAGED) {
    if (reason != nullptr) {
      *reason = str_sprintf("up to date: managed hw=%u %s v%s", static_cast<unsigned>(current.hardware_id),
                            firmware_mode_name_(current.mode_id),
                            format_firmware_version_(current.version).c_str());
    }
    return FirmwareAction::NONE;
  }

  if (reason != nullptr) {
    *reason = "unknown firmware state";
  }
  return FirmwareAction::UNKNOWN;
}

bool EmporiaVueComponent::bundled_firmware_available_() const { return this->bundled_firmware_size_() > 0; }

bool EmporiaVueComponent::bundled_firmware_matches_target_() const {
  return this->bundled_firmware_available_() &&
         (this->hardware_id_ == 0 || this->bundled_firmware_hardware_id_() == this->hardware_id_) &&
         this->bundled_firmware_mode_id_() == this->expected_firmware_mode_id_();
}

uint32_t EmporiaVueComponent::bundled_firmware_hardware_id_() const {
  return BUNDLED_SAMD_FIRMWARE_HARDWARE_ID;
}

uint32_t EmporiaVueComponent::bundled_firmware_mode_id_() const {
  return BUNDLED_SAMD_FIRMWARE_MODE_ID;
}

uint32_t EmporiaVueComponent::bundled_firmware_version_() const {
  return BUNDLED_SAMD_FIRMWARE_VERSION;
}

uint32_t EmporiaVueComponent::bundled_firmware_size_() const {
  return BUNDLED_SAMD_FIRMWARE_SIZE;
}

const uint8_t *EmporiaVueComponent::bundled_firmware_data_() const {
  return BUNDLED_SAMD_FIRMWARE;
}

bool EmporiaVueComponent::external_firmware_available_(uint8_t index) const {
  return index < EXTERNAL_SAMD_FIRMWARE_COUNT && EXTERNAL_SAMD_FIRMWARE_SIZES[index] > 0;
}

uint32_t EmporiaVueComponent::external_firmware_size_(uint8_t index) const {
  if (index >= EXTERNAL_SAMD_FIRMWARE_COUNT) {
    return 0;
  }
  return EXTERNAL_SAMD_FIRMWARE_SIZES[index];
}

const uint8_t *EmporiaVueComponent::external_firmware_data_(uint8_t index) const {
  if (index >= EXTERNAL_SAMD_FIRMWARE_COUNT) {
    return nullptr;
  }
  return EXTERNAL_SAMD_FIRMWARE_IMAGES[index];
}

uint16_t EmporiaVueComponent::expected_firmware_mode_id_() const {
  return this->runtime_mode_ == RuntimeMode::SPI ? MANAGED_MODE_SPI : MANAGED_MODE_I2C;
}

bool EmporiaVueComponent::nvm_wait_ready_() {
  for (uint16_t attempt = 0; attempt < 5000; attempt++) {
    uint8_t intflag = 0;
    if (!this->mem_read8_(NVMCTRL_INTFLAG, &intflag)) {
      return false;
    }
    if ((intflag & NVM_INTFLAG_ERROR) != 0) {
      uint16_t status = 0;
      this->mem_read16_(NVMCTRL_STATUS, &status);
      this->set_error_(str_sprintf("NVM error while waiting: INTFLAG=%s STATUS=%s", hex8_(intflag).c_str(),
                                   hex16_(status).c_str()));
      return false;
    }
    if ((intflag & NVM_INTFLAG_READY) != 0) {
      return true;
    }
    delayMicroseconds(100);
  }
  this->set_error_("timeout waiting for NVM ready");
  return false;
}

bool EmporiaVueComponent::nvm_clear_errors_() {
  bool ok = true;
  ok = this->mem_write8_(NVMCTRL_INTFLAG, NVM_INTFLAG_ERROR) && ok;
  ok = this->mem_write16_(NVMCTRL_STATUS, NVM_STATUS_ERROR_MASK) && ok;
  return ok;
}

bool EmporiaVueComponent::nvm_check_errors_() {
  uint8_t intflag = 0;
  uint16_t status = 0;
  if (!this->mem_read8_(NVMCTRL_INTFLAG, &intflag)) {
    return false;
  }
  if (!this->mem_read16_(NVMCTRL_STATUS, &status)) {
    return false;
  }
  if ((intflag & NVM_INTFLAG_ERROR) != 0 || (status & NVM_STATUS_ERROR_MASK) != 0) {
    this->set_error_(str_sprintf("NVM error: INTFLAG=%s STATUS=%s", hex8_(intflag).c_str(), hex16_(status).c_str()));
    return false;
  }
  return true;
}

bool EmporiaVueComponent::nvm_command_(uint8_t command) {
  if (!this->nvm_wait_ready_()) {
    return false;
  }
  if (!this->mem_write16_(NVMCTRL_CTRLA, NVM_CMD_KEY | static_cast<uint16_t>(command))) {
    return false;
  }
  if (!this->nvm_wait_ready_()) {
    return false;
  }
  return this->nvm_check_errors_();
}

bool EmporiaVueComponent::erase_flash_row_(uint32_t address) {
  if (!this->nvm_clear_errors_()) {
    return false;
  }
  if (!this->mem_write32_(NVMCTRL_ADDR, (address - FLASH_START) >> 1)) {
    return false;
  }
  return this->nvm_command_(NVM_CMD_ERASE_ROW);
}

bool EmporiaVueComponent::read_install_source_(uint32_t offset, uint32_t length, uint8_t *buffer) {
  if (length > BACKUP_IO_BLOCK_SIZE || offset > this->install_flash_size_ ||
      length > this->install_flash_size_ - offset) {
    this->set_error_("install source read out of range");
    return false;
  }

  if (this->install_source_ == FlashSource::BUNDLED) {
    if (offset > this->bundled_firmware_size_() || length > this->bundled_firmware_size_() - offset) {
      this->set_error_("bundled firmware read out of range");
      return false;
    }
    std::memcpy(buffer, this->bundled_firmware_data_() + offset, length);
    return true;
  }

  if (this->install_source_ == FlashSource::BACKUP) {
    if (this->backup_partition_ == nullptr && !this->find_backup_partition_()) {
      this->set_error_("backup partition not available");
      return false;
    }
    if (offset > this->install_backup_header_.flash_size ||
        length > this->install_backup_header_.flash_size - offset) {
      this->set_error_("backup firmware read out of range");
      return false;
    }
    const esp_err_t err = esp_partition_read(this->backup_partition_, this->install_backup_header_.image_offset + offset,
                                             buffer, length);
    if (err != ESP_OK) {
      this->set_error_(str_sprintf("backup partition read failed: 0x%X", static_cast<unsigned>(err)));
      return false;
    }
    return true;
  }

  if (this->install_source_ == FlashSource::EXTERNAL) {
    const uint32_t external_size = this->external_firmware_size_(this->install_external_firmware_index_);
    const uint8_t *external_data = this->external_firmware_data_(this->install_external_firmware_index_);
    if (external_data == nullptr || external_size > this->install_flash_size_) {
      this->set_error_("external firmware read out of range");
      return false;
    }
    std::memset(buffer, 0xFF, length);
    if (offset < external_size) {
      const uint32_t copy_len = std::min<uint32_t>(length, external_size - offset);
      std::memcpy(buffer, external_data + offset, copy_len);
    }
    return true;
  }

  this->set_error_("install source is not configured");
  return false;
}

bool EmporiaVueComponent::write_flash_page_(uint32_t address, uint32_t offset, uint32_t length) {
  uint8_t source[BACKUP_IO_BLOCK_SIZE]{};
  if (!this->read_install_source_(offset, length, source)) {
    return false;
  }

  if (!this->nvm_clear_errors_()) {
    return false;
  }
  if (!this->nvm_command_(NVM_CMD_PAGE_BUFFER_CLEAR)) {
    return false;
  }

  for (uint32_t i = 0; i < length; i += 4) {
    const uint32_t word = uint32_t(source[i]) | (uint32_t(source[i + 1]) << 8) |
                          (uint32_t(source[i + 2]) << 16) | (uint32_t(source[i + 3]) << 24);
    if (!this->mem_write32_(address + i, word)) {
      return false;
    }
  }

  if (!this->mem_write32_(NVMCTRL_ADDR, (address - FLASH_START) >> 1)) {
    return false;
  }
  return this->nvm_command_(NVM_CMD_WRITE_PAGE);
}

bool EmporiaVueComponent::verify_flash_page_(uint32_t address, uint32_t offset, uint32_t length) {
  uint8_t expected[BACKUP_IO_BLOCK_SIZE]{};
  uint8_t actual[BACKUP_IO_BLOCK_SIZE]{};
  if (length > sizeof(actual)) {
    this->set_error_("verify page too large");
    return false;
  }
  if (!this->read_install_source_(offset, length, expected)) {
    return false;
  }
  if (!this->read_flash_bytes_(address, static_cast<uint16_t>(length), actual)) {
    return false;
  }
  for (uint32_t i = 0; i < length; i++) {
    if (actual[i] != expected[i]) {
      this->set_error_(str_sprintf("verify mismatch at %s: read=%s expected=%s", hex32_(address + i).c_str(),
                                   hex8_(actual[i]).c_str(), hex8_(expected[i]).c_str()));
      return false;
    }
  }
  return true;
}

const char *EmporiaVueComponent::install_action_name_() const {
  switch (this->install_action_) {
    case FirmwareAction::UPDATE_MANAGED:
      return "bundled flash";
    case FirmwareAction::RESTORE_STOCK:
      return "backup flash";
    case FirmwareAction::FLASH_EXTERNAL:
      return "external flash";
    case FirmwareAction::NONE:
      return "none";
    case FirmwareAction::UNKNOWN:
    default:
      return "firmware action";
  }
}

void EmporiaVueComponent::process_install_() {
  if (this->install_stage_ != InstallStage::FLASH_PAGES) {
    this->fail_install_("internal install state error");
    return;
  }
  if (this->install_next_offset_ >= this->install_flash_size_) {
    this->finish_install_success_();
    return;
  }

  const uint32_t offset = this->install_next_offset_;
  const uint32_t address = FLASH_START + offset;
  const uint32_t length = std::min<uint32_t>(this->install_page_size_, this->install_flash_size_ - offset);

  if ((offset % this->install_row_size_) == 0) {
    this->install_started_writing_ = true;
    if (!this->erase_flash_row_(address)) {
      this->fail_install_("row erase failed at " + hex32_(address) + ": " + this->last_error_);
      return;
    }
  }

  if (!this->write_flash_page_(address, offset, length)) {
    this->fail_install_("page write failed at " + hex32_(address) + ": " + this->last_error_);
    return;
  }
  if (!this->verify_flash_page_(address, offset, length)) {
    this->fail_install_("page verify failed at " + hex32_(address) + ": " + this->last_error_);
    return;
  }

  this->install_next_offset_ += length;
  if (this->install_next_offset_ >= this->install_next_progress_log_offset_ ||
      this->install_next_offset_ >= this->install_flash_size_) {
    ESP_LOGI(TAG, "SAMD09 firmware %s progress: %" PRIu32 "/%" PRIu32 " bytes", this->install_action_name_(),
             this->install_next_offset_, this->install_flash_size_);
    while (this->install_next_progress_log_offset_ <= this->install_next_offset_) {
      this->install_next_progress_log_offset_ += INSTALL_PROGRESS_LOG_INTERVAL;
    }
  }
}

void EmporiaVueComponent::fail_install_(const std::string &error) {
  const std::string action_name = this->install_action_name_();
  const bool flash_may_be_partial = this->install_started_writing_;
  ESP_LOGW(TAG, "SAMD09 firmware %s failed: %s", action_name.c_str(), error.c_str());
  if (this->install_core_halted_) {
    if (flash_may_be_partial) {
      ESP_LOGW(TAG, "SAMD09 flash may be partially written; leaving core halted for recovery");
    } else if (!this->resume_core_()) {
      ESP_LOGW(TAG, "Failed to resume SAMD09 core after firmware %s failure: %s", action_name.c_str(),
               this->last_error_.c_str());
    }
  }

  this->install_active_ = false;
  this->install_core_halted_ = false;
  this->install_started_writing_ = false;
  this->install_action_ = FirmwareAction::UNKNOWN;
  this->install_source_ = FlashSource::NONE;
  this->install_backup_header_ = BackupHeader{};
  this->install_stage_ = InstallStage::IDLE;
  this->install_next_progress_log_offset_ = 0;
  this->install_external_firmware_index_ = 0;
  this->release_pins_();
  if (!flash_may_be_partial) {
    this->start_i2c_diagnostics_();
    this->start_metering_();
  }
}

void EmporiaVueComponent::finish_install_success_() {
  const FirmwareAction completed_action = this->install_action_;
  const std::string action_name = this->install_action_name_();
  const uint8_t completed_external_firmware_index = this->install_external_firmware_index_;
  FirmwareInfo info{};
  const bool info_ok =
      completed_action == FirmwareAction::FLASH_EXTERNAL ? false : this->read_current_firmware_info_(&info);
  if (info_ok) {
    this->detected_firmware_info_ = info;
    this->detected_firmware_info_valid_ = true;
    this->publish_firmware_version_(info);
  }

  if (completed_action == FirmwareAction::UPDATE_MANAGED) {
    if (!info_ok || info.kind != FirmwareKind::MANAGED || info.hardware_id != this->bundled_firmware_hardware_id_() ||
        info.mode_id != this->bundled_firmware_mode_id_() ||
        info.version != this->bundled_firmware_version_()) {
      this->fail_install_("final managed firmware marker verification failed");
      return;
    }
  } else if (completed_action == FirmwareAction::RESTORE_STOCK) {
    if (!info_ok || info.kind != FirmwareKind::STOCK) {
      this->fail_install_("final backup firmware verification failed");
      return;
    }
  } else if (completed_action == FirmwareAction::FLASH_EXTERNAL) {
    // The external image is intentionally treated as raw firmware. Page-level
    // readback already verified the bytes; no footer or version marker is required.
  } else {
    this->fail_install_("internal firmware action state error");
    return;
  }

  if (this->install_core_halted_) {
    if (!this->system_reset_core_()) {
      ESP_LOGW(TAG, "Failed to reset SAMD09 core after firmware %s: %s", action_name.c_str(),
               this->last_error_.c_str());
      if (!this->resume_core_()) {
        ESP_LOGW(TAG, "Failed to resume SAMD09 core after firmware %s: %s", action_name.c_str(),
                 this->last_error_.c_str());
      }
    }
  }

  this->install_active_ = false;
  this->install_core_halted_ = false;
  this->install_started_writing_ = false;
  this->install_action_ = FirmwareAction::UNKNOWN;
  this->install_source_ = FlashSource::NONE;
  this->install_backup_header_ = BackupHeader{};
  this->install_stage_ = InstallStage::IDLE;
  this->install_next_progress_log_offset_ = 0;
  this->install_external_firmware_index_ = 0;
  this->release_pins_();

  if (this->reset_pin_ != nullptr) {
    this->prepare_pins_();
    this->assert_reset_();
    this->deassert_reset_();
    this->release_pins_();
  }

  if (completed_action == FirmwareAction::UPDATE_MANAGED) {
    ESP_LOGI(TAG, "SAMD09 firmware update complete: hardware_id=%" PRIu32 ", version=%" PRIu32
                  " (v%s), mode=%s, size=%" PRIu32,
             this->bundled_firmware_hardware_id_(), this->bundled_firmware_version_(),
             format_firmware_version_(this->bundled_firmware_version_()).c_str(),
             firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
             this->bundled_firmware_size_());
    if (this->runtime_mode_ == RuntimeMode::I2C) {
      this->set_timeout("post_update_i2c_probe", 1000, [this]() { this->probe_runtime_i2c_after_firmware_update_(); });
    } else {
      this->start_i2c_diagnostics_();
      this->start_metering_();
    }
  } else if (completed_action == FirmwareAction::RESTORE_STOCK) {
    this->start_i2c_diagnostics_();
    this->start_metering_();
    ESP_LOGI(TAG, "SAMD09 backup firmware restore complete: size=%" PRIu32, this->install_flash_size_);
  } else if (completed_action == FirmwareAction::FLASH_EXTERNAL) {
    FirmwareInfo external_info{};
    external_info.kind = FirmwareKind::UNKNOWN;
    external_info.source = FirmwareDetectionSource::SWD;
    this->publish_firmware_version_(external_info);
    this->start_i2c_diagnostics_();
    this->start_metering_();
    ESP_LOGI(TAG, "SAMD09 external firmware flash complete: source_size=%" PRIu32 ", flash_size=%" PRIu32,
             this->external_firmware_size_(completed_external_firmware_index), this->install_flash_size_);
  }
}

void EmporiaVueComponent::process_backup_() {
  uint8_t buffer[BACKUP_IO_BLOCK_SIZE]{};
  const uint32_t remaining = this->backup_flash_size_ - this->backup_next_offset_;
  const uint16_t length = static_cast<uint16_t>(std::min<uint32_t>(sizeof(buffer), remaining));

  auto log_dump_chunk = [this](uint32_t offset, const uint8_t *data, uint16_t data_length) {
    std::string hex;
    hex.reserve(data_length * 2U);
    for (uint16_t i = 0; i < data_length; i++) {
      append_hex_byte_(&hex, data[i]);
    }
    ESP_LOGI(TAG, "SAMD09 firmware dump: offset=0x%05" PRIx32 " length=%u data=%s", offset,
             static_cast<unsigned>(data_length), hex.c_str());
  };

  if (length == 0) {
    this->fail_backup_("internal backup state error");
    return;
  }

  if (this->backup_stage_ == BackupStage::READ_AND_STORE) {
    if (!this->read_flash_bytes_(FLASH_START + this->backup_next_offset_, length, buffer)) {
      this->fail_backup_("flash read failed: " + this->last_error_);
      return;
    }
    log_dump_chunk(this->backup_next_offset_, buffer, length);
    const esp_err_t err =
        esp_partition_write(this->backup_partition_, BACKUP_IMAGE_OFFSET + this->backup_next_offset_, buffer, length);
    if (err != ESP_OK) {
      this->fail_backup_(str_sprintf("partition write failed: 0x%X", static_cast<unsigned>(err)));
      return;
    }

    this->backup_next_offset_ += length;

    if (this->backup_next_offset_ >= this->backup_flash_size_) {
      if (!this->hash_partition_image_(this->backup_flash_size_, this->backup_stored_hash_.data())) {
        this->fail_backup_("stored image hash failed");
        return;
      }
      if (!this->write_backup_hash_and_footer_(this->backup_stored_hash_.data(), this->backup_flash_size_)) {
        this->fail_backup_("failed to write backup hash/footer before verification");
        return;
      }
      if (!this->write_backup_state_(BACKUP_STATE_VALID)) {
        this->fail_backup_("failed to mark stored backup valid before verification");
        return;
      }
      this->backup_partition_finalized_ = true;
      const std::string stored_hash = sha256_hex_(this->backup_stored_hash_.data());
      ESP_LOGI(TAG,
               "SAMD09 partition backup valid before second read: size=%" PRIu32 ", sha256=%s; verifying SAMD",
               this->backup_flash_size_, stored_hash.c_str());
      mbedtls_sha256_init(&this->backup_sha_ctx_);
      mbedtls_sha256_starts(&this->backup_sha_ctx_, 0);
      this->backup_sha_ctx_active_ = true;
      this->backup_next_offset_ = 0;
      this->backup_stage_ = BackupStage::VERIFY_SECOND_READ;
    }
    return;
  }

  if (this->backup_stage_ == BackupStage::READ_AND_LOG) {
    if (!this->read_flash_bytes_(FLASH_START + this->backup_next_offset_, length, buffer)) {
      this->fail_backup_("flash dump read failed: " + this->last_error_);
      return;
    }

    log_dump_chunk(this->backup_next_offset_, buffer, length);
    mbedtls_sha256_update(&this->backup_sha_ctx_, buffer, length);

    this->backup_next_offset_ += length;

    if (this->backup_next_offset_ >= this->backup_flash_size_) {
      mbedtls_sha256_finish(&this->backup_sha_ctx_, this->backup_stored_hash_.data());
      mbedtls_sha256_free(&this->backup_sha_ctx_);
      this->backup_sha_ctx_active_ = false;
      mbedtls_sha256_init(&this->backup_sha_ctx_);
      mbedtls_sha256_starts(&this->backup_sha_ctx_, 0);
      this->backup_sha_ctx_active_ = true;
      this->backup_next_offset_ = 0;
      this->backup_stage_ = BackupStage::VERIFY_SECOND_READ;
      ESP_LOGI(TAG, "SAMD09 firmware dump complete; verifying with second read");
    }
    return;
  }

  if (this->backup_stage_ == BackupStage::VERIFY_SECOND_READ) {
    if (!this->read_flash_bytes_(FLASH_START + this->backup_next_offset_, length, buffer)) {
      this->fail_backup_("verify read failed: " + this->last_error_);
      return;
    }
    mbedtls_sha256_update(&this->backup_sha_ctx_, buffer, length);

    this->backup_next_offset_ += length;

    if (this->backup_next_offset_ >= this->backup_flash_size_) {
      mbedtls_sha256_finish(&this->backup_sha_ctx_, this->backup_verify_hash_.data());
      mbedtls_sha256_free(&this->backup_sha_ctx_);
      this->backup_sha_ctx_active_ = false;

      if (this->backup_stored_hash_ != this->backup_verify_hash_) {
        this->fail_backup_("hash mismatch between stored image and second SAMD read");
        return;
      }
      this->finish_backup_success_();
    }
    return;
  }

  this->fail_backup_("unknown backup stage");
}

void EmporiaVueComponent::fail_backup_(const std::string &error) {
  ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", error.c_str());
  if (this->backup_sha_ctx_active_) {
    mbedtls_sha256_free(&this->backup_sha_ctx_);
    this->backup_sha_ctx_active_ = false;
  }
  if (this->backup_header_written_) {
    if (!this->write_backup_state_(BACKUP_STATE_INVALID)) {
      ESP_LOGE(TAG, "Failed to mark SAMD09 backup invalid after verification failure");
    }
  }
  if (this->backup_core_halted_ && !this->resume_core_()) {
    ESP_LOGW(TAG, "Failed to resume SAMD09 core after backup failure: %s", this->last_error_.c_str());
  }
  this->backup_core_halted_ = false;
  this->backup_active_ = false;
  this->backup_stage_ = BackupStage::IDLE;
  this->backup_partition_finalized_ = false;
  this->backup_log_only_ = false;
  this->release_pins_();
  this->start_i2c_diagnostics_();
  this->start_metering_();
}

void EmporiaVueComponent::finish_backup_success_() {
  if (this->backup_log_only_) {
    if (this->backup_core_halted_ && !this->resume_core_()) {
      ESP_LOGW(TAG, "Failed to resume SAMD09 core after backup: %s", this->last_error_.c_str());
    }
    this->backup_core_halted_ = false;
    this->backup_active_ = false;
    this->backup_stage_ = BackupStage::IDLE;
    this->backup_log_only_ = false;
    this->release_pins_();
    this->start_i2c_diagnostics_();
    this->start_metering_();

    const std::string hash = sha256_hex_(this->backup_stored_hash_.data());
    ESP_LOGI(TAG, "SAMD09 firmware log dump verified: size=%" PRIu32 ", sha256=%s", this->backup_flash_size_,
             hash.c_str());
    return;
  }

  if (!this->backup_partition_finalized_) {
    this->fail_backup_("stored backup was not finalized before verification");
    return;
  }

  if (this->backup_core_halted_ && !this->resume_core_()) {
    ESP_LOGW(TAG, "Failed to resume SAMD09 core after backup: %s", this->last_error_.c_str());
  }
  this->backup_core_halted_ = false;
  this->backup_active_ = false;
  this->backup_stage_ = BackupStage::IDLE;
  this->backup_partition_finalized_ = false;
  this->release_pins_();
  this->start_i2c_diagnostics_();
  this->start_metering_();

  const std::string hash = sha256_hex_(this->backup_stored_hash_.data());
  ESP_LOGI(TAG, "SAMD09 legacy firmware backup valid: size=%" PRIu32 ", sha256=%s", this->backup_flash_size_,
           hash.c_str());
}

}  // namespace emporiavue
}  // namespace esphome
