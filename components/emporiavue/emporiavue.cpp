#include "emporiavue.h"
#include "samd09_firmware.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome {
namespace emporiavue {

static const char *const TAG = "emporiavue";

void EmporiaVueComponent::setup() {
  this->perform_boot_reset_();
  if (this->init_pins_on_boot_) {
    this->prepare_pins_();
    this->release_pins_();
  }
  this->inspect_backup_partition_();
  FirmwareInfo unknown_info{};
  this->publish_firmware_version_(unknown_info);
  if (this->diagnostics_status_sensor_ != nullptr) {
    this->diagnostics_status_sensor_->publish_state("unknown");
  }
  this->set_timeout("initial_firmware_detection", 5000, [this]() { this->publish_initial_firmware_detection_(); });
  if (this->runtime_mode_ == RuntimeMode::I2C) {
    this->set_timeout("initial_samd_i2c_diagnostics", 10000, [this]() { this->refresh_i2c_diagnostics_(); });
    this->set_interval("samd_i2c_diagnostics", DIAGNOSTICS_INTERVAL_MS, [this]() { this->refresh_i2c_diagnostics_(); });
  } else if (this->diagnostics_status_sensor_ != nullptr) {
    this->diagnostics_status_sensor_->publish_state("spi mode: i2c diagnostics disabled");
  }
}

void EmporiaVueComponent::loop() {
  if (this->backup_active_) {
    this->process_backup_();
    return;
  }
  if (this->install_active_) {
    this->process_install_();
    return;
  }
}

void EmporiaVueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EmporiaVue SAMD09 SWD reader:");
  LOG_PIN("  SWDIO Pin: ", this->swdio_pin_);
  LOG_PIN("  SWCLK Pin: ", this->swclk_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Reset before read: %s", YESNO(this->reset_before_read_));
  ESP_LOGCONFIG(TAG, "  Reset on boot: %s", YESNO(this->reset_on_boot_));
  ESP_LOGCONFIG(TAG, "  Connect under reset: %s", YESNO(this->connect_under_reset_));
  ESP_LOGCONFIG(TAG, "  Reset hold time: %" PRIu32 " ms", this->reset_hold_time_ms_);
  ESP_LOGCONFIG(TAG, "  Reset release time: %" PRIu32 " ms", this->reset_release_time_ms_);
  ESP_LOGCONFIG(TAG, "  Clock delay: %u us", this->clock_delay_us_);
  ESP_LOGCONFIG(TAG, "  Backup partition: %s", this->backup_partition_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Configured hardware id: %u", static_cast<unsigned>(this->hardware_id_));
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware hardware id: %" PRIu32, this->bundled_firmware_hardware_id_());
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware mode: %s (%" PRIu32 ")",
                firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                this->bundled_firmware_mode_id_());
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware version: v%s (raw %" PRIu32 ")",
                format_firmware_version_(this->bundled_firmware_version_()).c_str(), this->bundled_firmware_version_());
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware size: %" PRIu32 " bytes", this->bundled_firmware_size_());
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware compatible: %s",
                YESNO(this->bundled_firmware_matches_target_()));
  ESP_LOGCONFIG(TAG, "  Init pins on boot: %s", YESNO(this->init_pins_on_boot_));
  ESP_LOGCONFIG(TAG, "  Runtime mode: %s", this->runtime_mode_ == RuntimeMode::SPI ? "spi" : "i2c");
  ESP_LOGCONFIG(TAG, "  Auto-update SAMD: %s", YESNO(this->auto_update_samd_));
  const char *entity_prefix = this->entity_prefix_.empty() ? "(default)" : this->entity_prefix_.c_str();
  ESP_LOGCONFIG(TAG, "  Entity prefix: %s", entity_prefix);
  LOG_TEXT_SENSOR("  ", "Firmware status", this->firmware_status_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware version", this->firmware_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Diagnostics status", this->diagnostics_status_sensor_);
}

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
  this->backup_stage_ = BackupStage::IDLE;
  this->backup_next_offset_ = 0;
  this->backup_flash_size_ = 0;
  std::fill(this->backup_stored_hash_.begin(), this->backup_stored_hash_.end(), 0);
  std::fill(this->backup_verify_hash_.begin(), this->backup_verify_hash_.end(), 0);

  ESP_LOGI(TAG, "Starting SAMD09 legacy firmware backup");
  this->publish_firmware_status_("detecting firmware");
  this->publish_status_("backup: detecting firmware");

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    this->publish_firmware_status_("backup failed: SWD pins missing");
    return;
  }

  if (!this->find_backup_partition_()) {
    this->publish_firmware_status_("backup failed: partition missing");
    return;
  }

  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    this->release_pins_();
    this->publish_firmware_status_("backup failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    this->release_pins_();
    this->publish_firmware_status_("backup failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    this->release_pins_();
    this->publish_firmware_status_("backup failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware backup failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->halt_core_()) {
    this->release_pins_();
    this->publish_firmware_status_("backup failed: " + this->last_error_);
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
  this->publish_firmware_status_("legacy firmware detected");

  if (!this->backup_partition_has_capacity_(this->backup_flash_size_)) {
    this->fail_backup_("backup partition too small");
    return;
  }

  this->publish_firmware_status_("backup erasing partition");
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

  this->backup_next_offset_ = 0;
  this->backup_stage_ = BackupStage::READ_AND_STORE;
  this->backup_active_ = true;
  this->publish_firmware_status_(str_sprintf("backup reading 0/%" PRIu32, this->backup_flash_size_));
  ESP_LOGI(TAG,
           "SAMD09 legacy firmware backup started: flash_size=%" PRIu32 ", page_size=%" PRIu32
           ", page_count=%" PRIu32,
           this->backup_flash_size_, this->backup_page_size_, this->backup_page_count_);
}

void EmporiaVueComponent::install_firmware() { this->start_firmware_action_(FirmwareAction::UPDATE_MANAGED); }

void EmporiaVueComponent::restore_firmware() { this->start_firmware_action_(FirmwareAction::RESTORE_STOCK); }

void EmporiaVueComponent::start_firmware_action_(FirmwareAction requested_action) {
  if (requested_action != FirmwareAction::UPDATE_MANAGED && requested_action != FirmwareAction::RESTORE_STOCK) {
    ESP_LOGW(TAG, "Unsupported SAMD09 firmware action requested");
    return;
  }
  const bool restore_requested = requested_action == FirmwareAction::RESTORE_STOCK;
  const char *requested_name = restore_requested ? "restore" : "update";

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
  this->install_flash_size_ = 0;
  this->install_page_size_ = 0;
  this->install_row_size_ = 0;
  ESP_LOGI(TAG, "Starting SAMD09 firmware %s check", requested_name);
  this->publish_firmware_status_(std::string(requested_name) + " checking prerequisites");

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    this->publish_firmware_status_(std::string(requested_name) + " failed: SWD pins missing");
    return;
  }

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
  };

  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware update check failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware update check failed: %s", this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware update check failed: %s", this->last_error_.c_str());
    return;
  }

  if (!this->halt_core_()) {
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware update failed while halting core: %s", this->last_error_.c_str());
    return;
  }
  core_halted = true;

  FirmwareInfo current{};
  if (!this->read_current_firmware_info_(&current)) {
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware update check failed reading current firmware info: %s", this->last_error_.c_str());
    return;
  }
  this->detected_firmware_info_ = current;
  this->detected_firmware_info_valid_ = true;
  this->publish_firmware_version_(current);

  BackupHeader backup_header{};
  std::string backup_error;
  std::string action_reason;
  FirmwareAction selected_action = FirmwareAction::UNKNOWN;

  if (restore_requested) {
    if (!this->read_valid_backup_(&backup_header, &backup_error)) {
      release_after_check();
      this->publish_firmware_status_("restore blocked: valid backup required (" + backup_error + ")");
      ESP_LOGW(TAG, "SAMD09 firmware restore blocked: valid backup required (%s)", backup_error.c_str());
      return;
    }
    selected_action = FirmwareAction::RESTORE_STOCK;
    action_reason = "valid backup is available";
  } else {
    selected_action = this->determine_firmware_action_(current, &action_reason);
    if (selected_action == FirmwareAction::UNKNOWN) {
      release_after_check();
      this->publish_firmware_status_("update blocked: " + action_reason);
      ESP_LOGW(TAG, "SAMD09 firmware update blocked: %s", action_reason.c_str());
      return;
    }
    if (selected_action != FirmwareAction::UPDATE_MANAGED) {
      release_after_check();
      this->publish_firmware_status_("update not needed: " + action_reason);
      ESP_LOGI(TAG, "SAMD09 firmware update not needed: %s", action_reason.c_str());
      return;
    }

    if (!this->bundled_firmware_available_()) {
      release_after_check();
      this->publish_firmware_status_("update unavailable: no bundled managed firmware image is compiled in");
      ESP_LOGW(TAG, "SAMD09 managed firmware update requested but no bundled firmware image is compiled in");
      return;
    }

    if (!this->bundled_firmware_matches_target_()) {
      release_after_check();
      this->publish_firmware_status_(str_sprintf("update blocked: bundled image hw=%" PRIu32
                                                 " %s != configured hw=%u %s",
                                                 this->bundled_firmware_hardware_id_(),
                                                 firmware_mode_name_(
                                                     static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                                                 static_cast<unsigned>(this->hardware_id_),
                                                 firmware_mode_name_(this->expected_firmware_mode_id_())));
      ESP_LOGW(TAG, "SAMD09 firmware update blocked by bundled image target mismatch");
      return;
    }
  }

  const uint32_t source_size =
      selected_action == FirmwareAction::RESTORE_STOCK ? backup_header.flash_size : this->bundled_firmware_size_();
  if (source_size != current.flash_size) {
    release_after_check();
    this->publish_firmware_status_(str_sprintf("%s blocked: image size %" PRIu32 " != flash size %" PRIu32,
                                               requested_name, source_size, current.flash_size));
    ESP_LOGW(TAG, "SAMD09 firmware %s blocked by image/flash size mismatch", requested_name);
    return;
  }

  if (current.page_size == 0 || (current.page_size % 4U) != 0 || current.flash_size == 0 ||
      (current.flash_size % current.page_size) != 0 || current.page_size > BACKUP_IO_BLOCK_SIZE) {
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " blocked: unsupported flash geometry");
    ESP_LOGW(TAG, "SAMD09 firmware %s blocked by unsupported flash geometry", requested_name);
    return;
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
  this->install_source_ = selected_action == FirmwareAction::RESTORE_STOCK ? FlashSource::BACKUP : FlashSource::BUNDLED;
  if (selected_action == FirmwareAction::RESTORE_STOCK) {
    this->install_backup_header_ = backup_header;
  }
  this->install_flash_size_ = current.flash_size;
  this->install_page_size_ = current.page_size;
  this->install_row_size_ = current.page_size * NVM_PAGES_PER_ROW;
  this->install_next_offset_ = 0;
  this->install_stage_ = InstallStage::FLASH_PAGES;
  this->install_active_ = true;
  this->publish_status_(std::string(requested_name) + " SAMD firmware");
  this->publish_firmware_status_(str_sprintf("%s flashing 0/%" PRIu32, requested_name, this->install_flash_size_));
  ESP_LOGI(TAG,
           "SAMD09 firmware %s started: current_kind=%s, current_version=%" PRIu32
           ", source_size=%" PRIu32 ", reason=%s",
           requested_name,
           current.kind == FirmwareKind::MANAGED ? "managed" : (current.kind == FirmwareKind::STOCK ? "stock" : "unknown"),
           current.version, source_size, action_reason.c_str());
}

void EmporiaVueComponent::reset_target_() {
  if (!this->reset_before_read_ || this->reset_pin_ == nullptr) {
    return;
  }
  this->assert_reset_();
  this->deassert_reset_();
}

void EmporiaVueComponent::assert_reset_() {
  this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->reset_pin_->digital_write(false);
  this->target_reset_asserted_ = true;
  delay(this->reset_hold_time_ms_);
}

void EmporiaVueComponent::deassert_reset_() {
  this->reset_pin_->digital_write(true);
  this->target_reset_asserted_ = false;
  delay(this->reset_release_time_ms_);
}

void EmporiaVueComponent::deassert_reset_for_swd_attach_() {
  this->reset_pin_->digital_write(true);
  this->target_reset_asserted_ = false;
}

bool EmporiaVueComponent::connect_under_reset_active_() const {
  return this->connect_under_reset_ && this->reset_pin_ != nullptr;
}

void EmporiaVueComponent::cold_plug_swd_() {
  // SAMD09 cold-plugging is detected when RESET is released while SWCLK is low.
  this->swclk_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->swclk_pin_->digital_write(false);
  if (!this->target_reset_asserted_) {
    this->assert_reset_();
  }
  ESP_LOGI(TAG, "Releasing SAMD09 reset with SWCLK low for cold-plug");
  this->deassert_reset_for_swd_attach_();
  delay(this->reset_release_time_ms_);
}

void EmporiaVueComponent::begin_swd_session_() {
  if (this->connect_under_reset_active_()) {
    ESP_LOGI(TAG, "Holding SAMD09 SWCLK low and asserting reset for connect-under-reset");
    this->swclk_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->swclk_pin_->digital_write(false);
    this->assert_reset_();
    return;
  }
  if (this->connect_under_reset_ && this->reset_pin_ == nullptr) {
    ESP_LOGW(TAG, "connect_under_reset is enabled but reset_pin is not configured");
  }
  this->reset_target_();
}

void EmporiaVueComponent::finish_swd_session_() {
  if (this->connect_under_reset_active_() && this->target_reset_asserted_) {
    ESP_LOGI(TAG, "Releasing SAMD09 reset after connect-under-reset");
    this->deassert_reset_();
  }
}

void EmporiaVueComponent::perform_boot_reset_() {
  if (!this->reset_on_boot_) {
    return;
  }
  if (this->reset_pin_ == nullptr) {
    ESP_LOGW(TAG, "reset_on_boot is enabled but reset_pin is not configured");
    return;
  }

  ESP_LOGI(TAG, "Resetting SAMD09 on ESP32 boot for %" PRIu32 " ms", this->reset_hold_time_ms_);
  this->reset_pin_->digital_write(true);
  this->reset_pin_->setup();
  this->assert_reset_();
  this->deassert_reset_();
  this->reset_pin_->digital_write(true);
  this->reset_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
}

void EmporiaVueComponent::swd_enter_debug_(uint8_t swj_select_bits) {
  this->direction_write_ = false;
  this->selected_ap_valid_ = false;
  this->cached_csw_ = 0xFFFFFFFFUL;

  this->swdio_pin_->digital_write(true);
  this->swdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->swclk_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->swclk_pin_->digital_write(true);

  this->write_bits_(0xFFFFFFFFUL, 32);
  this->write_bits_(0xFFFFFFFFUL, 32);
  if (swj_select_bits > 0) {
    this->write_bits_(0xE79EUL, swj_select_bits);
    this->write_bits_(0xFFFFFFFFUL, 32);
    this->write_bits_(0xFFFFFFFFUL, 32);
  }
  this->write_bits_(0x00UL, 32);
  this->write_bits_(0x00UL, 32);
}

bool EmporiaVueComponent::swd_initialize_(uint32_t *idcode) {
  struct ProbeVariant {
    const char *name;
    uint8_t swj_select_bits;
    bool sample_before_clock;
  };
  const ProbeVariant variants[] = {
      {"SWD line reset, DAPLink sample", 0, false},
      {"SWJ JTAG-to-SWD 16-bit, DAPLink sample", 16, false},
      {"SWD line reset, ATC sample", 0, true},
      {"SWJ JTAG-to-SWD 16-bit, ATC sample", 16, true},
      {"SWJ JTAG-to-SWD 32-bit, odewdney sample", 32, true},
  };
  for (const auto &variant : variants) {
    ESP_LOGV(TAG, "Trying SAMD09 %s initialization", variant.name);
    this->last_error_.clear();
    this->sample_before_clock_ = variant.sample_before_clock;
    if (this->connect_under_reset_active_()) {
      this->cold_plug_swd_();
    }
    this->swd_enter_debug_(variant.swj_select_bits);
    if (!this->dp_read_(DP_IDCODE, idcode)) {
      continue;
    }
    if (*idcode == 0x00000000UL || *idcode == 0xFFFFFFFFUL) {
      this->set_error_(str_sprintf("invalid SWD IDCODE %s", hex32_(*idcode).c_str()));
      continue;
    }
    return true;
  }
  if (this->last_error_.empty()) {
    this->set_error_("SAMD09 SWD initialization failed");
  }
  return false;
}

void EmporiaVueComponent::prepare_pins_() {
  const bool use_reset_pin = (this->reset_before_read_ || this->connect_under_reset_) && this->reset_pin_ != nullptr;
  if (use_reset_pin) {
    this->reset_pin_->digital_write(true);
  }
  if (this->swclk_pin_ != nullptr) {
    this->swclk_pin_->digital_write(true);
  }
  if (this->swdio_pin_ != nullptr) {
    this->swdio_pin_->digital_write(true);
  }

  if (!this->pins_setup_) {
    if (use_reset_pin) {
      this->reset_pin_->setup();
    }
    if (this->swclk_pin_ != nullptr) {
      this->swclk_pin_->setup();
    }
    if (this->swdio_pin_ != nullptr) {
      this->swdio_pin_->setup();
    }
    this->pins_setup_ = true;
  }

  if (use_reset_pin) {
    this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->reset_pin_->digital_write(true);
  }
  if (this->swclk_pin_ != nullptr) {
    this->swclk_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->swclk_pin_->digital_write(true);
  }
  if (this->swdio_pin_ != nullptr) {
    this->swdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
}

void EmporiaVueComponent::release_pins_() {
  if ((this->reset_before_read_ || this->connect_under_reset_) && this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    this->target_reset_asserted_ = false;
    this->reset_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
  if (this->swclk_pin_ != nullptr) {
    this->swclk_pin_->digital_write(true);
    this->swclk_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
  if (this->swdio_pin_ != nullptr) {
    this->swdio_pin_->digital_write(true);
    this->swdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }
}

void EmporiaVueComponent::set_error_(const std::string &error) {
  this->last_error_ = error;
  ESP_LOGW(TAG, "%s", error.c_str());
}

void EmporiaVueComponent::publish_status_(const std::string &) {}

void EmporiaVueComponent::publish_firmware_status_(const std::string &status) {
  if (this->firmware_status_sensor_ != nullptr) {
    this->firmware_status_sensor_->publish_state(status);
  }
}

void EmporiaVueComponent::publish_firmware_version_(const FirmwareInfo &info) {
  if (this->firmware_version_sensor_ == nullptr) {
    return;
  }
  switch (info.kind) {
    case FirmwareKind::MANAGED:
      this->firmware_version_sensor_->publish_state(
          str_sprintf("v%s (%s)", format_firmware_version_(info.version).c_str(), firmware_mode_name_(info.mode_id)));
      break;
    case FirmwareKind::STOCK:
      this->firmware_version_sensor_->publish_state("stock");
      break;
    case FirmwareKind::UNKNOWN:
    default:
      this->firmware_version_sensor_->publish_state("unknown");
      break;
  }
}

std::string EmporiaVueComponent::hex32_(uint32_t value) { return str_sprintf("0x%08" PRIx32, value); }

std::string EmporiaVueComponent::hex16_(uint16_t value) { return str_sprintf("0x%04" PRIx16, value); }

std::string EmporiaVueComponent::hex8_(uint8_t value) { return str_sprintf("0x%02" PRIx8, value); }

void EmporiaVueComponent::append_hex_byte_(std::string *output, uint8_t value) {
  static const char HEX[] = "0123456789abcdef";
  output->push_back(HEX[(value >> 4) & 0x0F]);
  output->push_back(HEX[value & 0x0F]);
}

std::string EmporiaVueComponent::sha256_hex_(const uint8_t hash[32]) {
  std::string output;
  output.reserve(64);
  for (uint8_t i = 0; i < 32; i++) {
    append_hex_byte_(&output, hash[i]);
  }
  return output;
}

std::string EmporiaVueComponent::format_firmware_version_(uint32_t version) {
  return str_sprintf("%" PRIu32 ".%" PRIu32, version / 10U, version % 10U);
}

const char *EmporiaVueComponent::firmware_mode_name_(uint16_t mode_id) {
  switch (mode_id) {
    case MANAGED_MODE_I2C:
      return "i2c";
    case MANAGED_MODE_SPI:
      return "spi";
    default:
      return "unknown";
  }
}

uint32_t EmporiaVueComponent::crc32_(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t index = 0; index < length; index++) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

void EmporiaVueComponent::clock_half_period_() {
  if (this->clock_delay_us_ > 0) {
    delayMicroseconds(this->clock_delay_us_);
  }
}

void EmporiaVueComponent::swclk_pulse_() {
  this->swclk_pin_->digital_write(false);
  this->clock_half_period_();
  this->swclk_pin_->digital_write(true);
  this->clock_half_period_();
}

void EmporiaVueComponent::turnaround_(bool write) {
  this->swdio_pin_->digital_write(true);
  this->swdio_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->swclk_pulse_();
  if (write) {
    this->swdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  }
  this->direction_write_ = write;
}

void EmporiaVueComponent::write_bits_(uint32_t data, uint8_t bits) {
  if (!this->direction_write_) {
    this->turnaround_(true);
  }
  for (uint8_t i = 0; i < bits; i++) {
    this->swdio_pin_->digital_write((data & 0x01UL) != 0);
    this->swclk_pulse_();
    data >>= 1;
  }
}

uint32_t EmporiaVueComponent::read_bits_(uint8_t bits) {
  if (this->direction_write_) {
    this->turnaround_(false);
  }
  uint32_t value = 0;
  for (uint8_t i = 0; i < bits; i++) {
    if (this->sample_before_clock_ && this->swdio_pin_->digital_read()) {
      value |= 1UL << i;
    }
    this->swclk_pin_->digital_write(false);
    this->clock_half_period_();
    if (!this->sample_before_clock_ && this->swdio_pin_->digital_read()) {
      value |= 1UL << i;
    }
    this->swclk_pin_->digital_write(true);
    this->clock_half_period_();
  }
  return value;
}

bool EmporiaVueComponent::parity32_(uint32_t value) {
  value ^= value >> 16;
  value ^= value >> 8;
  value ^= value >> 4;
  value &= 0x0FUL;
  return ((0x6996U >> value) & 1U) != 0;
}

uint8_t EmporiaVueComponent::make_request_(bool ap, bool read, uint8_t addr) {
  const uint8_t a2 = (addr >> 2) & 0x01;
  const uint8_t a3 = (addr >> 3) & 0x01;
  const uint8_t parity = (ap ? 1 : 0) ^ (read ? 1 : 0) ^ a2 ^ a3;
  return 0x81 | (ap ? 0x02 : 0x00) | (read ? 0x04 : 0x00) | (a2 << 3) | (a3 << 4) | (parity << 5);
}

bool EmporiaVueComponent::transfer_(bool ap, bool read, uint8_t addr, uint32_t write_value, uint32_t *read_value,
                                         uint8_t *ack) {
  this->write_bits_(this->make_request_(ap, read, addr), 8);
  *ack = this->read_bits_(3);

  if (*ack == SWD_ACK_OK) {
    if (read) {
      const uint32_t value = this->read_bits_(32);
      const bool parity = this->read_bits_(1) != 0;
      this->write_bits_(0, 8);
      if (parity != parity32_(value)) {
        this->last_error_ = str_sprintf("parity error reading %s register 0x%02X", ap ? "AP" : "DP", addr);
        ESP_LOGD(TAG, "%s", this->last_error_.c_str());
        return false;
      }
      if (read_value != nullptr) {
        *read_value = value;
      }
    } else {
      this->write_bits_(write_value, 32);
      this->write_bits_(parity32_(write_value) ? 1 : 0, 1);
      this->write_bits_(0, 8);
    }
    return true;
  }

  this->write_bits_(0, 8);
  return false;
}

bool EmporiaVueComponent::dp_read_(uint8_t addr, uint32_t *value) {
  for (uint8_t attempt = 0; attempt < this->retry_count_; attempt++) {
    uint8_t ack = 0;
    if (this->transfer_(false, true, addr, 0, value, &ack)) {
      return true;
    }
    if (ack == SWD_ACK_WAIT) {
      delayMicroseconds(50);
      continue;
    }
    if (ack == SWD_ACK_OK) {
      delayMicroseconds(50);
      continue;
    }
    if (ack == SWD_ACK_FAULT) {
      this->clear_sticky_errors_();
      this->set_error_(str_sprintf("SWD FAULT reading DP register 0x%02X", addr));
      return false;
    }
    this->set_error_(str_sprintf("bad SWD ACK 0x%02X reading DP register 0x%02X", ack, addr));
    return false;
  }
  this->set_error_(str_sprintf("timeout reading DP register 0x%02X", addr));
  return false;
}

bool EmporiaVueComponent::dp_write_(uint8_t addr, uint32_t value) {
  for (uint8_t attempt = 0; attempt < this->retry_count_; attempt++) {
    uint8_t ack = 0;
    uint32_t unused = 0;
    if (this->transfer_(false, false, addr, value, &unused, &ack)) {
      return true;
    }
    if (ack == SWD_ACK_WAIT) {
      delayMicroseconds(50);
      continue;
    }
    if (ack == SWD_ACK_FAULT) {
      this->clear_sticky_errors_();
      this->set_error_(str_sprintf("SWD FAULT writing DP register 0x%02X", addr));
      return false;
    }
    this->set_error_(str_sprintf("bad SWD ACK 0x%02X writing DP register 0x%02X", ack, addr));
    return false;
  }
  this->set_error_(str_sprintf("timeout writing DP register 0x%02X", addr));
  return false;
}

bool EmporiaVueComponent::clear_sticky_errors_() {
  uint8_t ack = 0;
  uint32_t unused = 0;
  return this->transfer_(false, false, DP_ABORT, 0x0000001EUL, &unused, &ack) && ack == SWD_ACK_OK;
}

bool EmporiaVueComponent::select_ap_bank_(uint8_t bank) {
  if (this->selected_ap_valid_ && this->selected_ap_bank_ == bank) {
    return true;
  }
  if (!this->dp_write_(DP_SELECT, (uint32_t(bank & 0x0F) << 4))) {
    return false;
  }
  this->selected_ap_valid_ = true;
  this->selected_ap_bank_ = bank;
  return true;
}

bool EmporiaVueComponent::ap_read_(uint8_t addr, uint32_t *value) {
  if (!this->select_ap_bank_((addr >> 4) & 0x0F)) {
    return false;
  }
  uint32_t posted = 0;
  bool request_accepted = false;
  for (uint8_t attempt = 0; attempt < this->retry_count_; attempt++) {
    uint8_t ack = 0;
    if (this->transfer_(true, true, addr & 0x0C, 0, &posted, &ack)) {
      request_accepted = true;
      break;
    }
    if (ack == SWD_ACK_WAIT) {
      delayMicroseconds(50);
      continue;
    }
    if (ack == SWD_ACK_OK) {
      delayMicroseconds(50);
      continue;
    }
    if (ack == SWD_ACK_FAULT) {
      this->clear_sticky_errors_();
      this->set_error_(str_sprintf("SWD FAULT reading AP register 0x%02X", addr));
      return false;
    }
    this->set_error_(str_sprintf("bad SWD ACK 0x%02X reading AP register 0x%02X", ack, addr));
    return false;
  }
  if (!request_accepted) {
    this->set_error_(str_sprintf("timeout reading AP register 0x%02X", addr));
    return false;
  }
  return this->dp_read_(DP_RDBUFF, value);
}

bool EmporiaVueComponent::ap_write_(uint8_t addr, uint32_t value) {
  if (!this->select_ap_bank_((addr >> 4) & 0x0F)) {
    return false;
  }
  for (uint8_t attempt = 0; attempt < this->retry_count_; attempt++) {
    uint8_t ack = 0;
    uint32_t unused = 0;
    if (this->transfer_(true, false, addr & 0x0C, value, &unused, &ack)) {
      return true;
    }
    if (ack == SWD_ACK_WAIT) {
      delayMicroseconds(50);
      continue;
    }
    if (ack == SWD_ACK_FAULT) {
      this->clear_sticky_errors_();
      this->set_error_(str_sprintf("SWD FAULT writing AP register 0x%02X", addr));
      return false;
    }
    this->set_error_(str_sprintf("bad SWD ACK 0x%02X writing AP register 0x%02X", ack, addr));
    return false;
  }
  this->set_error_(str_sprintf("timeout writing AP register 0x%02X", addr));
  return false;
}

bool EmporiaVueComponent::mem_read_(uint32_t address, MemSize size, uint32_t *value) {
  const uint32_t csw = MEM_AP_CSW_BASE | uint32_t(size);
  if (this->cached_csw_ != csw) {
    if (!this->ap_write_(AP_CSW, csw)) {
      return false;
    }
    this->cached_csw_ = csw;
  }
  if (!this->ap_write_(AP_TAR, address)) {
    return false;
  }
  return this->ap_read_(AP_DRW, value);
}

bool EmporiaVueComponent::mem_read8_(uint32_t address, uint8_t *value) {
  uint32_t raw = 0;
  if (!this->mem_read_(address, MEM_SIZE_BYTE, &raw)) {
    return false;
  }
  *value = static_cast<uint8_t>(raw & 0xFFU);
  return true;
}

bool EmporiaVueComponent::mem_read16_(uint32_t address, uint16_t *value) {
  uint32_t raw = 0;
  if (!this->mem_read_(address, MEM_SIZE_HALFWORD, &raw)) {
    return false;
  }
  *value = static_cast<uint16_t>(raw & 0xFFFFU);
  return true;
}

bool EmporiaVueComponent::mem_read32_(uint32_t address, uint32_t *value) {
  return this->mem_read_(address, MEM_SIZE_WORD, value);
}

bool EmporiaVueComponent::mem_write_(uint32_t address, MemSize size, uint32_t value) {
  const uint32_t csw = MEM_AP_CSW_BASE | uint32_t(size);
  if (this->cached_csw_ != csw) {
    if (!this->ap_write_(AP_CSW, csw)) {
      return false;
    }
    this->cached_csw_ = csw;
  }
  if (!this->ap_write_(AP_TAR, address)) {
    return false;
  }
  return this->ap_write_(AP_DRW, value);
}

bool EmporiaVueComponent::mem_write8_(uint32_t address, uint8_t value) {
  return this->mem_write_(address, MEM_SIZE_BYTE, value);
}

bool EmporiaVueComponent::mem_write16_(uint32_t address, uint16_t value) {
  return this->mem_write_(address, MEM_SIZE_HALFWORD, value);
}

bool EmporiaVueComponent::mem_write32_(uint32_t address, uint32_t value) {
  return this->mem_write_(address, MEM_SIZE_WORD, value);
}

bool EmporiaVueComponent::halt_core_() {
  ESP_LOGD(TAG, "Halting SAMD09 core");
  return this->mem_write32_(DHCSR, DHCSR_DBGKEY | DHCSR_C_DEBUGEN | DHCSR_C_HALT);
}

bool EmporiaVueComponent::resume_core_() {
  ESP_LOGD(TAG, "Resuming SAMD09 core");
  return this->mem_write32_(DHCSR, DHCSR_DBGKEY | DHCSR_C_DEBUGEN);
}

bool EmporiaVueComponent::system_reset_core_() {
  ESP_LOGD(TAG, "Requesting SAMD09 system reset");
  return this->mem_write32_(AIRCR, AIRCR_VECTKEY | AIRCR_SYSRESETREQ);
}

bool EmporiaVueComponent::read_core_register_(uint8_t reg, uint32_t *value) {
  if (!this->mem_write32_(DCRSR, reg)) {
    return false;
  }
  for (uint8_t attempt = 0; attempt < 50; attempt++) {
    uint32_t dhcsr = 0;
    if (!this->mem_read32_(DHCSR, &dhcsr)) {
      return false;
    }
    if ((dhcsr & DHCSR_S_REGRDY) != 0) {
      return this->mem_read32_(DCRDR, value);
    }
    delayMicroseconds(100);
  }
  this->set_error_(str_sprintf("timeout reading core register %u", static_cast<unsigned>(reg)));
  return false;
}

bool EmporiaVueComponent::read_flash_geometry_(uint32_t *param, uint32_t *page_size, uint32_t *page_count,
                                               uint32_t *flash_size) {
  uint32_t raw_param = 0;
  if (!this->mem_read32_(NVMCTRL_PARAM, &raw_param)) {
    return false;
  }

  const uint32_t computed_page_size = 8UL << ((raw_param >> 16) & 0x07U);
  const uint32_t computed_page_count = raw_param & 0xFFFFU;
  if (computed_page_size == 0 || computed_page_count == 0) {
    this->set_error_(str_sprintf("invalid NVM PARAM %s", hex32_(raw_param).c_str()));
    return false;
  }

  const uint64_t computed_flash_size = uint64_t(computed_page_size) * uint64_t(computed_page_count);
  if (computed_flash_size > 0xFFFFFFFFULL) {
    this->set_error_(str_sprintf("NVM flash size too large: page_size=%" PRIu32 ", page_count=%" PRIu32,
                                 computed_page_size, computed_page_count));
    return false;
  }

  *param = raw_param;
  *page_size = computed_page_size;
  *page_count = computed_page_count;
  *flash_size = static_cast<uint32_t>(computed_flash_size);
  return true;
}

bool EmporiaVueComponent::read_flash_bytes_(uint32_t address, uint16_t length, uint8_t *data) {
  uint16_t offset = 0;
  while (offset < length) {
    const uint32_t current_address = address + offset;
    const uint16_t remaining = length - offset;
    if ((current_address & 0x03U) == 0 && remaining >= 4) {
      uint32_t word = 0;
      if (!this->mem_read32_(current_address, &word)) {
        return false;
      }
      data[offset++] = static_cast<uint8_t>(word & 0xFFU);
      data[offset++] = static_cast<uint8_t>((word >> 8) & 0xFFU);
      data[offset++] = static_cast<uint8_t>((word >> 16) & 0xFFU);
      data[offset++] = static_cast<uint8_t>((word >> 24) & 0xFFU);
    } else {
      if (!this->mem_read8_(current_address, &data[offset])) {
        return false;
      }
      offset++;
    }
  }
  return true;
}

bool EmporiaVueComponent::flash_row_erased_(uint32_t address, uint32_t row_size, bool *erased) {
  *erased = false;
  uint8_t buffer[BACKUP_IO_BLOCK_SIZE]{};
  uint32_t offset = 0;
  while (offset < row_size) {
    const uint16_t length = static_cast<uint16_t>(std::min<uint32_t>(sizeof(buffer), row_size - offset));
    if (!this->read_flash_bytes_(address + offset, length, buffer)) {
      return false;
    }
    for (uint16_t i = 0; i < length; i++) {
      if (buffer[i] != 0xFF) {
        return true;
      }
    }
    offset += length;
  }
  *erased = true;
  return true;
}

bool EmporiaVueComponent::power_up_debug_() {
  if (!this->dp_write_(DP_CTRL_STAT, DP_POWER_REQUEST)) {
    return false;
  }
  for (uint8_t attempt = 0; attempt < this->retry_count_; attempt++) {
    uint32_t status = 0;
    if (!this->dp_read_(DP_CTRL_STAT, &status)) {
      return false;
    }
    if ((status & DP_POWER_ACK) == DP_POWER_ACK) {
      return true;
    }
    delayMicroseconds(100);
  }
  this->set_error_("debug power-up ACK timeout");
  return false;
}

bool EmporiaVueComponent::verify_mem_ap_() {
  uint32_t idr = 0;
  if (!this->ap_read_(AP_IDR, &idr)) {
    return false;
  }
  const uint8_t ap_class = (idr >> 13) & 0x0F;
  if (ap_class != 0x08) {
    this->set_error_(str_sprintf("unexpected MEM-AP IDR %s", hex32_(idr).c_str()));
    return false;
  }
  ESP_LOGD(TAG, "MEM-AP IDR=%s", hex32_(idr).c_str());
  return true;
}

void EmporiaVueComponent::inspect_backup_partition_() {
  if (!this->find_backup_partition_()) {
    return;
  }

  BackupHeader header{};
  if (!this->read_backup_header_(&header) || header.magic != BACKUP_MAGIC) {
    this->publish_firmware_status_("backup missing");
    return;
  }

  if (header.state == BACKUP_STATE_IN_PROGRESS) {
    this->publish_firmware_status_("backup incomplete");
    return;
  }
  if (header.state == BACKUP_STATE_INVALID) {
    this->publish_firmware_status_("backup invalid");
    return;
  }
  if (header.state != BACKUP_STATE_VALID) {
    this->publish_firmware_status_("backup unknown state");
    return;
  }
  if (header.version != BACKUP_HEADER_VERSION || header.header_size != sizeof(BackupHeader) ||
      !this->backup_partition_has_capacity_(header.flash_size)) {
    this->publish_firmware_status_("backup invalid metadata");
    return;
  }

  BackupFooter footer{};
  const uint32_t footer_offset = header.image_offset + header.flash_size;
  if (esp_partition_read(this->backup_partition_, footer_offset, &footer, sizeof(footer)) != ESP_OK ||
      footer.magic != BACKUP_FOOTER_MAGIC || footer.flash_size != header.flash_size ||
      std::memcmp(footer.sha256, header.sha256, sizeof(header.sha256)) != 0) {
    this->publish_firmware_status_("backup invalid footer");
    return;
  }

  uint8_t hash[32]{};
  if (!this->hash_partition_image_(header.flash_size, hash)) {
    this->publish_firmware_status_("backup hash read failed");
    return;
  }
  if (std::memcmp(hash, header.sha256, sizeof(hash)) != 0) {
    this->publish_firmware_status_("backup hash mismatch");
    return;
  }

  this->publish_firmware_status_("backup valid sha256=" + sha256_hex_(hash).substr(0, 12));
}

bool EmporiaVueComponent::find_backup_partition_() {
  this->backup_partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                                     this->backup_partition_name_.c_str());
  if (this->backup_partition_ == nullptr) {
    ESP_LOGW(TAG, "SAMD backup partition '%s' not found", this->backup_partition_name_.c_str());
    this->publish_firmware_status_("backup partition missing: " + this->backup_partition_name_);
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

EmporiaVueComponent::ManagedI2CDiagnosticResult EmporiaVueComponent::query_managed_i2c_diagnostic_(
    ManagedI2CDiagnostic *diagnostic) {
  ManagedI2CDiagnostic candidate{};
  const uint8_t command = MANAGED_I2C_DIAGNOSTIC_COMMAND;
  const i2c::ErrorCode error =
      this->write_read(&command, 1, reinterpret_cast<uint8_t *>(&candidate), sizeof(candidate));
  if (error != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "SAMD09 managed I2C diagnostic query failed: i2c error %u", static_cast<unsigned>(error));
    return ManagedI2CDiagnosticResult::I2C_ERROR;
  }
  if (!this->validate_managed_i2c_diagnostic_(candidate)) {
    ESP_LOGD(TAG, "SAMD09 managed I2C diagnostic query returned no valid managed firmware response");
    return ManagedI2CDiagnosticResult::INVALID_RESPONSE;
  }

  *diagnostic = candidate;
  ESP_LOGD(TAG,
           "SAMD09 managed I2C diagnostic: seq=%" PRIu32 ", samples=%" PRIu32 ", built=%" PRIu32
           ", read=%" PRIu32 ", dma_errors=%" PRIu32 ", overruns=%" PRIu32,
           candidate.diagnostic_sequence, candidate.sample_blocks, candidate.packets_built, candidate.packets_read,
           candidate.dma_transfer_errors, candidate.packet_overruns);
  return ManagedI2CDiagnosticResult::VALID_RESPONSE;
}

bool EmporiaVueComponent::validate_managed_i2c_diagnostic_(const ManagedI2CDiagnostic &diagnostic) const {
  const uint32_t expected_crc =
      crc32_(reinterpret_cast<const uint8_t *>(&diagnostic), offsetof(ManagedI2CDiagnostic, crc32));
  if (expected_crc != diagnostic.crc32) {
    return false;
  }
  if (diagnostic.hardware_id != 2 && diagnostic.hardware_id != 3) {
    return false;
  }
  if (diagnostic.firmware_version == 0) {
    return false;
  }
  return diagnostic.i2c_frame_length == STOCK_I2C_FRAME_SIZE;
}

void EmporiaVueComponent::publish_firmware_info_from_diagnostic_(const ManagedI2CDiagnostic &diagnostic) {
  FirmwareInfo info{};
  info.kind = FirmwareKind::MANAGED;
  info.hardware_id = diagnostic.hardware_id;
  info.mode_id = MANAGED_MODE_I2C;
  info.version = diagnostic.firmware_version;
  info.i2c_frame_length = diagnostic.i2c_frame_length;
  info.source = FirmwareDetectionSource::I2C;
  this->detected_firmware_info_ = info;
  this->detected_firmware_info_valid_ = true;
  this->publish_firmware_version_(info);
}

void EmporiaVueComponent::refresh_i2c_diagnostics_() {
  if (this->backup_active_ || this->install_active_) {
    return;
  }
  if (this->runtime_mode_ != RuntimeMode::I2C) {
    if (this->diagnostics_status_sensor_ != nullptr) {
      this->diagnostics_status_sensor_->publish_state("unavailable: spi mode");
    }
    return;
  }
  if (!this->detected_firmware_info_valid_ || this->detected_firmware_info_.kind != FirmwareKind::MANAGED) {
    if (this->diagnostics_status_sensor_ != nullptr) {
      this->diagnostics_status_sensor_->publish_state("unavailable: managed firmware not detected");
    }
    return;
  }

  ManagedI2CDiagnostic diagnostic{};
  const ManagedI2CDiagnosticResult result = this->query_managed_i2c_diagnostic_(&diagnostic);
  if (result == ManagedI2CDiagnosticResult::VALID_RESPONSE) {
    this->publish_firmware_info_from_diagnostic_(diagnostic);
    this->publish_i2c_diagnostics_(diagnostic);
  } else if (this->diagnostics_status_sensor_ != nullptr) {
    this->diagnostics_status_sensor_->publish_state(
        result == ManagedI2CDiagnosticResult::I2C_ERROR ? "failed: i2c error" : "failed: invalid response");
  }
}

void EmporiaVueComponent::publish_i2c_diagnostics_(const ManagedI2CDiagnostic &diagnostic) {
  if (this->diagnostics_status_sensor_ != nullptr) {
    this->diagnostics_status_sensor_->publish_state(str_sprintf(
        "ok: seq=%" PRIu32 " dma_errors=%" PRIu32 " overruns=%" PRIu32 " partial_reads=%" PRIu32,
        diagnostic.diagnostic_sequence, diagnostic.dma_transfer_errors, diagnostic.packet_overruns,
        diagnostic.i2c_partial_reads));
  }
  if (this->diag_sample_blocks_sensor_ != nullptr) {
    this->diag_sample_blocks_sensor_->publish_state(static_cast<float>(diagnostic.sample_blocks));
  }
  if (this->diag_packets_built_sensor_ != nullptr) {
    this->diag_packets_built_sensor_->publish_state(static_cast<float>(diagnostic.packets_built));
  }
  if (this->diag_packets_read_sensor_ != nullptr) {
    this->diag_packets_read_sensor_->publish_state(static_cast<float>(diagnostic.packets_read));
  }
  if (this->diag_dma_transfer_errors_sensor_ != nullptr) {
    this->diag_dma_transfer_errors_sensor_->publish_state(static_cast<float>(diagnostic.dma_transfer_errors));
  }
  if (this->diag_packet_overruns_sensor_ != nullptr) {
    this->diag_packet_overruns_sensor_->publish_state(static_cast<float>(diagnostic.packet_overruns));
  }
  if (this->diag_i2c_partial_reads_sensor_ != nullptr) {
    this->diag_i2c_partial_reads_sensor_->publish_state(static_cast<float>(diagnostic.i2c_partial_reads));
  }
  if (this->diag_i2c_oversize_reads_sensor_ != nullptr) {
    this->diag_i2c_oversize_reads_sensor_->publish_state(static_cast<float>(diagnostic.i2c_oversize_reads));
  }
  if (this->diag_last_sample_count_sensor_ != nullptr) {
    this->diag_last_sample_count_sensor_->publish_state(static_cast<float>(diagnostic.last_sample_count));
  }
  if (this->diag_last_i2c_read_len_sensor_ != nullptr) {
    this->diag_last_i2c_read_len_sensor_->publish_state(static_cast<float>(diagnostic.last_i2c_read_len));
  }
}

i2c::ErrorCode EmporiaVueComponent::read_normal_i2c_frame_(const char *context) {
  std::array<uint8_t, STOCK_I2C_FRAME_SIZE> frame{};
  const i2c::ErrorCode error = this->read(frame.data(), frame.size());
  if (error != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "SAMD09 normal I2C frame read failed (%s): i2c error %u", context,
             static_cast<unsigned>(error));
    return error;
  }

  std::string first16;
  for (size_t i = 0; i < std::min<size_t>(16, frame.size()); i++) {
    append_hex_byte_(&first16, frame[i]);
  }

  size_t nonzero = 0;
  size_t non_ff = 0;
  for (uint8_t byte : frame) {
    if (byte != 0x00) {
      nonzero++;
    }
    if (byte != 0xFF) {
      non_ff++;
    }
  }

  const uint16_t end_word = uint16_t(frame[STOCK_I2C_FRAME_SIZE - 2]) |
                            (uint16_t(frame[STOCK_I2C_FRAME_SIZE - 1]) << 8);
  ESP_LOGI(TAG,
           "SAMD09 normal I2C frame read OK (%s): len=%u unread=%u checksum=%s unknown=%s sequence=%u end=%s "
           "nonzero=%u non_ff=%u first16=%s",
           context, static_cast<unsigned>(frame.size()), static_cast<unsigned>(frame[0]), hex8_(frame[1]).c_str(),
           hex8_(frame[2]).c_str(), static_cast<unsigned>(frame[3]), hex16_(end_word).c_str(),
           static_cast<unsigned>(nonzero), static_cast<unsigned>(non_ff), first16.c_str());
  return i2c::ERROR_OK;
}

void EmporiaVueComponent::probe_runtime_i2c_after_firmware_update_() {
  if (this->runtime_mode_ != RuntimeMode::I2C) {
    this->publish_firmware_status_("update complete; spi mode active");
    return;
  }

  ManagedI2CDiagnostic diagnostic{};
  const ManagedI2CDiagnosticResult info_result = this->query_managed_i2c_diagnostic_(&diagnostic);
  if (info_result == ManagedI2CDiagnosticResult::VALID_RESPONSE) {
    this->publish_firmware_info_from_diagnostic_(diagnostic);
    this->publish_i2c_diagnostics_(diagnostic);
    ESP_LOGI(TAG, "SAMD09 runtime managed I2C diagnostic OK after update");
  } else {
    ESP_LOGW(TAG, "SAMD09 runtime managed I2C diagnostic failed after update: result=%u",
             static_cast<unsigned>(info_result));
  }

  const i2c::ErrorCode frame_error = this->read_normal_i2c_frame_("post-update");
  if (frame_error == i2c::ERROR_OK) {
    this->publish_firmware_status_("update complete; normal i2c frame ok");
  } else {
    this->publish_firmware_status_(str_sprintf("update complete; normal i2c frame failed: i2c error %u",
                                               static_cast<unsigned>(frame_error)));
  }
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
    this->publish_firmware_status_("firmware detection unavailable: " + error);
    return;
  }

  this->detected_firmware_info_ = info;
  this->detected_firmware_info_valid_ = true;
  this->publish_firmware_version_(info);
  if (info.kind == FirmwareKind::MANAGED) {
    this->publish_firmware_status_("managed firmware detected by swd footer");
  } else {
    this->publish_firmware_status_("stock firmware detected by swd flash check");
  }

  if (!this->auto_update_samd_) {
    return;
  }

  std::string auto_update_reason;
  if (!this->should_auto_update_samd_(info, &auto_update_reason)) {
    ESP_LOGI(TAG, "SAMD09 auto-update not needed: %s", auto_update_reason.c_str());
    return;
  }

  ESP_LOGI(TAG, "SAMD09 auto-update starting: %s", auto_update_reason.c_str());
  this->publish_firmware_status_("auto update: " + auto_update_reason);
  this->start_firmware_action_(FirmwareAction::UPDATE_MANAGED);
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

bool EmporiaVueComponent::bundled_firmware_available_() const { return BUNDLED_SAMD_FIRMWARE_SIZE > 0; }

bool EmporiaVueComponent::bundled_firmware_matches_target_() const {
  return this->bundled_firmware_available_() &&
         (this->hardware_id_ == 0 || this->bundled_firmware_hardware_id_() == this->hardware_id_) &&
         this->bundled_firmware_mode_id_() == this->expected_firmware_mode_id_();
}

uint32_t EmporiaVueComponent::bundled_firmware_hardware_id_() const { return BUNDLED_SAMD_FIRMWARE_HARDWARE_ID; }

uint32_t EmporiaVueComponent::bundled_firmware_mode_id_() const { return BUNDLED_SAMD_FIRMWARE_MODE_ID; }

uint32_t EmporiaVueComponent::bundled_firmware_version_() const { return BUNDLED_SAMD_FIRMWARE_VERSION; }

uint32_t EmporiaVueComponent::bundled_firmware_size_() const { return BUNDLED_SAMD_FIRMWARE_SIZE; }

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
    std::memcpy(buffer, BUNDLED_SAMD_FIRMWARE + offset, length);
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
      return "update";
    case FirmwareAction::RESTORE_STOCK:
      return "restore";
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
  if ((this->install_next_offset_ % 4096U) == 0 || this->install_next_offset_ >= this->install_flash_size_) {
    this->publish_firmware_status_(str_sprintf("%s flashing %" PRIu32 "/%" PRIu32, this->install_action_name_(),
                                               this->install_next_offset_, this->install_flash_size_));
  }
}

void EmporiaVueComponent::fail_install_(const std::string &error) {
  const std::string action_name = this->install_action_name_();
  ESP_LOGW(TAG, "SAMD09 firmware %s failed: %s", action_name.c_str(), error.c_str());
  if (this->install_core_halted_) {
    if (this->install_started_writing_) {
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
  this->release_pins_();
  this->publish_status_(action_name + " failed: " + error);
  this->publish_firmware_status_(action_name + " failed: " + error);
}

void EmporiaVueComponent::finish_install_success_() {
  const FirmwareAction completed_action = this->install_action_;
  const std::string action_name = this->install_action_name_();
  FirmwareInfo info{};
  const bool info_ok = this->read_current_firmware_info_(&info);
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
  this->release_pins_();

  if (this->reset_pin_ != nullptr) {
    this->prepare_pins_();
    this->assert_reset_();
    this->deassert_reset_();
    this->release_pins_();
  }

  if (completed_action == FirmwareAction::UPDATE_MANAGED) {
    this->publish_status_("update complete");
    this->publish_firmware_status_(
        str_sprintf("update complete: managed hw=%" PRIu32 " %s v%s",
                    this->bundled_firmware_hardware_id_(),
                    firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                    format_firmware_version_(this->bundled_firmware_version_()).c_str()));
    ESP_LOGI(TAG, "SAMD09 managed firmware update complete: hardware_id=%" PRIu32 ", version=%" PRIu32
                  " (v%s), mode=%s, size=%" PRIu32,
             this->bundled_firmware_hardware_id_(), this->bundled_firmware_version_(),
             format_firmware_version_(this->bundled_firmware_version_()).c_str(),
             firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
             this->bundled_firmware_size_());
    if (this->runtime_mode_ == RuntimeMode::I2C) {
      this->set_timeout("post_update_i2c_probe", 1000, [this]() { this->probe_runtime_i2c_after_firmware_update_(); });
    } else {
      this->publish_firmware_status_("update complete: spi mode active");
    }
  } else if (completed_action == FirmwareAction::RESTORE_STOCK) {
    this->publish_status_("restore complete");
    this->publish_firmware_status_("restore complete: backup firmware");
    ESP_LOGI(TAG, "SAMD09 backup firmware restore complete: size=%" PRIu32, this->install_flash_size_);
  }
}

void EmporiaVueComponent::process_backup_() {
  uint8_t buffer[BACKUP_IO_BLOCK_SIZE]{};
  const uint32_t remaining = this->backup_flash_size_ - this->backup_next_offset_;
  const uint16_t length = static_cast<uint16_t>(std::min<uint32_t>(sizeof(buffer), remaining));

  if (length == 0) {
    this->fail_backup_("internal backup state error");
    return;
  }

  if (this->backup_stage_ == BackupStage::READ_AND_STORE) {
    if (!this->read_flash_bytes_(FLASH_START + this->backup_next_offset_, length, buffer)) {
      this->fail_backup_("flash read failed: " + this->last_error_);
      return;
    }
    const esp_err_t err =
        esp_partition_write(this->backup_partition_, BACKUP_IMAGE_OFFSET + this->backup_next_offset_, buffer, length);
    if (err != ESP_OK) {
      this->fail_backup_(str_sprintf("partition write failed: 0x%X", static_cast<unsigned>(err)));
      return;
    }

    this->backup_next_offset_ += length;
    if ((this->backup_next_offset_ % 1024U) == 0 || this->backup_next_offset_ >= this->backup_flash_size_) {
      this->publish_firmware_status_(str_sprintf("backup reading %" PRIu32 "/%" PRIu32, this->backup_next_offset_,
                                                 this->backup_flash_size_));
    }

    if (this->backup_next_offset_ >= this->backup_flash_size_) {
      if (!this->hash_partition_image_(this->backup_flash_size_, this->backup_stored_hash_.data())) {
        this->fail_backup_("stored image hash failed");
        return;
      }
      mbedtls_sha256_init(&this->backup_sha_ctx_);
      mbedtls_sha256_starts(&this->backup_sha_ctx_, 0);
      this->backup_sha_ctx_active_ = true;
      this->backup_next_offset_ = 0;
      this->backup_stage_ = BackupStage::VERIFY_SECOND_READ;
      this->publish_firmware_status_(str_sprintf("backup verifying 0/%" PRIu32, this->backup_flash_size_));
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
    if ((this->backup_next_offset_ % 1024U) == 0 || this->backup_next_offset_ >= this->backup_flash_size_) {
      this->publish_firmware_status_(str_sprintf("backup verifying %" PRIu32 "/%" PRIu32, this->backup_next_offset_,
                                                 this->backup_flash_size_));
    }

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
    this->write_backup_state_(BACKUP_STATE_INVALID);
  }
  if (this->backup_core_halted_ && !this->resume_core_()) {
    ESP_LOGW(TAG, "Failed to resume SAMD09 core after backup failure: %s", this->last_error_.c_str());
  }
  this->backup_core_halted_ = false;
  this->backup_active_ = false;
  this->backup_stage_ = BackupStage::IDLE;
  this->release_pins_();
  this->publish_status_("backup failed: " + error);
  this->publish_firmware_status_("backup failed: " + error);
}

void EmporiaVueComponent::finish_backup_success_() {
  if (!this->write_backup_hash_and_footer_(this->backup_stored_hash_.data(), this->backup_flash_size_)) {
    this->fail_backup_("failed to write backup hash/footer");
    return;
  }
  if (!this->write_backup_state_(BACKUP_STATE_VALID)) {
    this->fail_backup_("failed to mark backup valid");
    return;
  }

  if (this->backup_core_halted_ && !this->resume_core_()) {
    ESP_LOGW(TAG, "Failed to resume SAMD09 core after backup: %s", this->last_error_.c_str());
  }
  this->backup_core_halted_ = false;
  this->backup_active_ = false;
  this->backup_stage_ = BackupStage::IDLE;
  this->release_pins_();

  const std::string hash = sha256_hex_(this->backup_stored_hash_.data());
  this->publish_status_("backup valid");
  this->publish_firmware_status_("backup valid sha256=" + hash.substr(0, 12));
  ESP_LOGI(TAG, "SAMD09 legacy firmware backup valid: size=%" PRIu32 ", sha256=%s", this->backup_flash_size_,
           hash.c_str());
}

}  // namespace emporiavue
}  // namespace esphome
