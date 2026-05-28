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
  this->publish_firmware_update_available_(false);
  this->publish_firmware_restore_available_(false);
  this->publish_firmware_action_("unknown");
  FirmwareInfo unknown_info{};
  this->publish_firmware_version_(unknown_info);
  this->set_timeout("initial_firmware_detection", 5000, [this]() { this->publish_initial_firmware_detection_(); });
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

  if (!this->dump_active_) {
    return;
  }

  const uint32_t block = this->dump_next_block_;
  const uint32_t address = this->dump_start_address_ + (uint32_t(block) * this->dump_block_size_);
  uint16_t length = this->dump_block_size_;
  if (this->dump_total_size_ > 0) {
    const uint32_t bytes_read = block * uint32_t(this->dump_block_size_);
    const uint32_t remaining = this->dump_total_size_ - bytes_read;
    if (remaining < length) {
      length = static_cast<uint16_t>(remaining);
    }
  }

  if (this->dump_halt_core_ && !this->dump_core_halted_) {
    if (!this->halt_core_()) {
      this->dump_active_ = false;
      this->release_pins_();
      this->publish_status_("failed: " + this->last_error_);
      ESP_LOGW(TAG, "SAMD09 flash dump failed before block=%" PRIu32 ": %s", block,
               this->last_error_.c_str());
      return;
    }
    this->dump_core_halted_ = true;
  }

  std::string hex_data;
  if (!this->dump_flash_block_(address, length, &hex_data)) {
    this->dump_active_ = false;
    if (this->dump_core_halted_ && !this->resume_core_()) {
      ESP_LOGW(TAG, "Failed to resume SAMD09 core after dump error: %s", this->last_error_.c_str());
    }
    this->dump_core_halted_ = false;
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 flash dump failed at block=%" PRIu32 " addr=%s: %s", block,
             hex32_(address).c_str(), this->last_error_.c_str());
    return;
  }

  if (this->dump_core_halted_ && this->dump_resume_between_blocks_) {
    if (!this->resume_core_()) {
      this->dump_active_ = false;
      this->dump_core_halted_ = false;
      this->release_pins_();
      this->publish_status_("failed: " + this->last_error_);
      ESP_LOGW(TAG, "SAMD09 flash dump failed resuming after block=%" PRIu32 ": %s", block,
               this->last_error_.c_str());
      return;
    }
    this->dump_core_halted_ = false;
  }

  ESP_LOGI(TAG, "SAMD09_FLASH_DUMP block=%04" PRIu32 " addr=%s len=%u data=%s", block, hex32_(address).c_str(),
           static_cast<unsigned>(length), hex_data.c_str());
  this->dump_next_block_++;

  if (this->dump_next_block_ >= this->dump_effective_block_count_) {
    this->dump_active_ = false;
    if (this->dump_core_halted_ && !this->resume_core_()) {
      ESP_LOGW(TAG, "Failed to resume SAMD09 core after flash dump: %s", this->last_error_.c_str());
    }
    this->dump_core_halted_ = false;
    this->release_pins_();
    this->publish_status_("flash dump done");
    ESP_LOGI(TAG, "SAMD09 flash dump complete: blocks=%" PRIu32 ", block_size=%u, total_size=%" PRIu32,
             this->dump_effective_block_count_, static_cast<unsigned>(this->dump_block_size_), this->dump_total_size_);
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
  ESP_LOGCONFIG(TAG, "  Dump start address: %s", hex32_(this->dump_start_address_).c_str());
  ESP_LOGCONFIG(TAG, "  Dump block size: %u bytes", static_cast<unsigned>(this->dump_block_size_));
  ESP_LOGCONFIG(TAG, "  Dump block count: %" PRIu32, this->dump_block_count_);
  ESP_LOGCONFIG(TAG, "  Dump full flash: %s", YESNO(this->dump_full_flash_));
  ESP_LOGCONFIG(TAG, "  Dump halt core: %s", YESNO(this->dump_halt_core_));
  ESP_LOGCONFIG(TAG, "  Dump resume between blocks: %s", YESNO(this->dump_resume_between_blocks_));
  ESP_LOGCONFIG(TAG, "  Backup partition: %s", this->backup_partition_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Configured hardware id: %u", static_cast<unsigned>(this->hardware_id_));
  ESP_LOGCONFIG(TAG, "  Required managed firmware version: v%s (raw %" PRIu32 ")",
                format_firmware_version_(this->required_firmware_version_).c_str(), this->required_firmware_version_);
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware hardware id: %" PRIu32, this->bundled_firmware_hardware_id_());
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware version: v%s (raw %" PRIu32 ")",
                format_firmware_version_(this->bundled_firmware_version_()).c_str(), this->bundled_firmware_version_());
  ESP_LOGCONFIG(TAG, "  Bundled managed firmware size: %" PRIu32 " bytes", this->bundled_firmware_size_());
  ESP_LOGCONFIG(TAG, "  SAMD writes enabled: %s", YESNO(this->allow_samd_write_));
  ESP_LOGCONFIG(TAG, "  Require backup before install: %s", YESNO(this->require_backup_before_install_));
  ESP_LOGCONFIG(TAG, "  Init pins on boot: %s", YESNO(this->init_pins_on_boot_));
  LOG_TEXT_SENSOR("  ", "SWD IDCODE", this->swd_idcode_sensor_);
  LOG_TEXT_SENSOR("  ", "DSU DID", this->dsu_did_sensor_);
  LOG_TEXT_SENSOR("  ", "Status", this->status_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware status", this->firmware_status_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware action", this->firmware_action_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware version", this->firmware_version_sensor_);
  LOG_BINARY_SENSOR("  ", "Read allowed", this->read_allowed_sensor_);
  LOG_BINARY_SENSOR("  ", "Firmware update available", this->firmware_update_available_sensor_);
  LOG_BINARY_SENSOR("  ", "Firmware restore available", this->firmware_restore_available_sensor_);
}

void EmporiaVueComponent::read_samd() {
  if (this->install_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware install is running; read check ignored");
    return;
  }
  if (this->dump_active_) {
    ESP_LOGW(TAG, "SAMD09 flash dump is running; read check ignored");
    return;
  }

  this->last_error_.clear();
  ESP_LOGI(TAG, "Starting SAMD09 SWD read check");
  this->publish_status_("reading");
  this->publish_read_allowed_(false);

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    return;
  }

  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 read check failed: %s", this->last_error_.c_str());
    return;
  }
  if (this->swd_idcode_sensor_ != nullptr) {
    this->swd_idcode_sensor_->publish_state(hex32_(swd_idcode));
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 read check failed: %s", this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 read check failed: %s", this->last_error_.c_str());
    return;
  }

  uint8_t dsu_statusb = 0;
  if (!this->mem_read8_(DSU_STATUSB, &dsu_statusb)) {
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 read check failed: %s", this->last_error_.c_str());
    return;
  }

  uint32_t dsu_did = 0;
  if (this->mem_read32_(DSU_DID, &dsu_did)) {
    if (this->dsu_did_sensor_ != nullptr) {
      this->dsu_did_sensor_->publish_state(hex32_(dsu_did));
    }
  } else if (this->dsu_did_sensor_ != nullptr) {
    this->dsu_did_sensor_->publish_state("unreadable");
  }

  const bool dsu_protected = (dsu_statusb & 0x01) != 0;
  bool read_allowed = !dsu_protected;
  std::string status = str_sprintf("DSU STATUSB=%s, PROT=%u", hex8_(dsu_statusb).c_str(), dsu_protected ? 1 : 0);

  if (read_allowed) {
    uint32_t flash_probe = 0;
    if (!this->mem_read32_(FLASH_START, &flash_probe)) {
      read_allowed = false;
      status += ", flash probe failed";
      ESP_LOGW(TAG, "SAMD09 flash probe failed: %s", this->last_error_.c_str());
    } else {
      uint16_t nvm_status = 0;
      if (this->mem_read16_(NVMCTRL_STATUS, &nvm_status)) {
        const bool nvm_security_bit = (nvm_status & (1U << 8)) != 0;
        read_allowed = read_allowed && !nvm_security_bit;
        status += str_sprintf(", NVM STATUS=%s, SB=%u", hex16_(nvm_status).c_str(), nvm_security_bit ? 1 : 0);
      } else {
        status += ", NVM STATUS unreadable";
      }
    }
  }

  this->release_pins_();
  this->publish_read_allowed_(read_allowed);
  this->publish_status_(status);
  ESP_LOGI(TAG, "SAMD09 read check: SWD IDCODE=%s, DSU DID=%s, read_allowed=%s, %s", hex32_(swd_idcode).c_str(),
           hex32_(dsu_did).c_str(), YESNO(read_allowed), status.c_str());
}

void EmporiaVueComponent::probe_swd() {
  if (this->install_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware install is running; SWD probe ignored");
    return;
  }
  if (this->dump_active_) {
    ESP_LOGW(TAG, "SAMD09 flash dump is running; SWD probe ignored");
    return;
  }

  this->last_error_.clear();
  ESP_LOGI(TAG, "Starting SAMD09 SWD probe");

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    return;
  }

  this->prepare_pins_();
  const bool swdio_idle = this->swdio_pin_->digital_read();
  ESP_LOGI(TAG, "SAMD09 SWD line state before probe: SWDIO=%u", swdio_idle ? 1 : 0);
  this->begin_swd_session_();

  uint8_t ack = 0;
  uint32_t swd_idcode = 0;
  if (this->probe_idcode_("SWD line reset, DAPLink sample", 0, false, &swd_idcode, &ack) ||
      this->probe_idcode_("SWJ JTAG-to-SWD 16-bit, DAPLink sample", 16, false, &swd_idcode, &ack) ||
      this->probe_idcode_("SWD line reset, ATC sample", 0, true, &swd_idcode, &ack) ||
      this->probe_idcode_("SWJ JTAG-to-SWD 16-bit, ATC sample", 16, true, &swd_idcode, &ack) ||
      this->probe_idcode_("SWJ JTAG-to-SWD 32-bit, odewdney sample", 32, true, &swd_idcode, &ack)) {
    this->finish_swd_session_();
    this->release_pins_();
    if (this->swd_idcode_sensor_ != nullptr) {
      this->swd_idcode_sensor_->publish_state(hex32_(swd_idcode));
    }
    ESP_LOGI(TAG, "SAMD09 SWD probe OK: DP IDCODE=%s, ACK=%s", hex32_(swd_idcode).c_str(), hex8_(ack).c_str());
    return;
  }

  this->finish_swd_session_();
  this->release_pins_();
  if (ack == SWD_ACK_WAIT) {
    ESP_LOGW(TAG, "SAMD09 SWD probe got WAIT ACK=%s while reading DP IDCODE", hex8_(ack).c_str());
  } else if (ack == SWD_ACK_FAULT) {
    ESP_LOGW(TAG, "SAMD09 SWD probe got FAULT ACK=%s while reading DP IDCODE", hex8_(ack).c_str());
  } else if (!this->last_error_.empty()) {
    ESP_LOGW(TAG, "SAMD09 SWD probe failed: ACK=%s, %s", hex8_(ack).c_str(), this->last_error_.c_str());
  } else {
    ESP_LOGW(TAG, "SAMD09 SWD probe failed: invalid ACK=%s while reading DP IDCODE", hex8_(ack).c_str());
  }
}

void EmporiaVueComponent::dump_flash() {
  if (this->install_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware install is running; flash dump ignored");
    return;
  }
  if (this->backup_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware backup is running; flash dump ignored");
    return;
  }
  if (this->dump_active_) {
    ESP_LOGW(TAG, "SAMD09 flash dump is already running");
    return;
  }
  this->dump_core_halted_ = false;

  this->last_error_.clear();
  this->dump_effective_block_count_ = this->dump_block_count_;
  this->dump_total_size_ = this->dump_block_count_ * uint32_t(this->dump_block_size_);
  ESP_LOGI(TAG, "Starting SAMD09 flash dump: start=%s, blocks=%" PRIu32 ", block_size=%u, full_flash=%s",
           hex32_(this->dump_start_address_).c_str(), this->dump_block_count_,
           static_cast<unsigned>(this->dump_block_size_), YESNO(this->dump_full_flash_));
  this->publish_status_("dumping flash");

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    this->publish_status_("failed: " + this->last_error_);
    return;
  }

  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 flash dump failed: %s", this->last_error_.c_str());
    return;
  }
  if (this->swd_idcode_sensor_ != nullptr) {
    this->swd_idcode_sensor_->publish_state(hex32_(swd_idcode));
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 flash dump failed: %s", this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    this->release_pins_();
    this->publish_status_("failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 flash dump failed: %s", this->last_error_.c_str());
    return;
  }

  if (this->dump_halt_core_) {
    if (!this->halt_core_()) {
      this->release_pins_();
      this->publish_status_("failed: " + this->last_error_);
      ESP_LOGW(TAG, "SAMD09 flash dump failed while halting core: %s", this->last_error_.c_str());
      return;
    }
    this->dump_core_halted_ = true;
  }

  if (this->dump_full_flash_) {
    uint32_t nvm_param = 0;
    uint32_t page_size = 0;
    uint32_t page_count = 0;
    uint32_t flash_size = 0;
    if (!this->read_flash_geometry_(&nvm_param, &page_size, &page_count, &flash_size)) {
      const std::string error = this->last_error_;
      if (this->dump_core_halted_ && !this->resume_core_()) {
        ESP_LOGW(TAG, "Failed to resume SAMD09 core after geometry read error: %s", this->last_error_.c_str());
      }
      this->dump_core_halted_ = false;
      this->release_pins_();
      this->publish_status_("failed: " + error);
      ESP_LOGW(TAG, "SAMD09 flash dump failed reading flash geometry: %s", error.c_str());
      return;
    }
    if (this->dump_start_address_ >= flash_size) {
      this->set_error_(str_sprintf("dump_start_address %s is outside flash size %" PRIu32,
                                   hex32_(this->dump_start_address_).c_str(), flash_size));
      const std::string error = this->last_error_;
      if (this->dump_core_halted_ && !this->resume_core_()) {
        ESP_LOGW(TAG, "Failed to resume SAMD09 core after flash geometry error: %s", this->last_error_.c_str());
      }
      this->dump_core_halted_ = false;
      this->release_pins_();
      this->publish_status_("failed: " + error);
      ESP_LOGW(TAG, "SAMD09 flash dump failed: %s", error.c_str());
      return;
    }
    this->dump_total_size_ = flash_size - this->dump_start_address_;
    this->dump_effective_block_count_ =
        (this->dump_total_size_ + uint32_t(this->dump_block_size_) - 1U) / uint32_t(this->dump_block_size_);
    ESP_LOGI(TAG,
             "SAMD09 flash geometry: NVM PARAM=%s, page_size=%" PRIu32 ", page_count=%" PRIu32
             ", flash_size=%" PRIu32 ", dump_size=%" PRIu32 ", blocks=%" PRIu32,
             hex32_(nvm_param).c_str(), page_size, page_count, flash_size, this->dump_total_size_,
             this->dump_effective_block_count_);
  }

  this->dump_next_block_ = 0;
  this->dump_active_ = true;
  ESP_LOGI(TAG, "SAMD09 flash dump job started; %" PRIu32 " blocks will be read one per loop cycle",
           this->dump_effective_block_count_);
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
  if (this->dump_active_) {
    ESP_LOGW(TAG, "SAMD09 flash dump is running; firmware backup ignored");
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
  if (this->swd_idcode_sensor_ != nullptr) {
    this->swd_idcode_sensor_->publish_state(hex32_(swd_idcode));
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

  if (this->mem_read32_(DSU_DID, &this->backup_dsu_did_)) {
    if (this->dsu_did_sensor_ != nullptr) {
      this->dsu_did_sensor_->publish_state(hex32_(this->backup_dsu_did_));
    }
  } else {
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

void EmporiaVueComponent::test_flash_write() {
  if (this->install_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware install is running; test write ignored");
    return;
  }
  if (this->backup_active_) {
    ESP_LOGW(TAG, "SAMD09 firmware backup is running; test write ignored");
    return;
  }
  if (this->dump_active_) {
    ESP_LOGW(TAG, "SAMD09 flash dump is running; test write ignored");
    return;
  }

  this->last_error_.clear();
  ESP_LOGI(TAG, "Starting SAMD09 flash write test");
  this->publish_status_("test write: checking");
  this->publish_firmware_status_("test write checking");

  if (!this->allow_samd_write_) {
    this->publish_firmware_status_("test write blocked: allow_samd_write is false");
    ESP_LOGW(TAG, "SAMD09 flash write test blocked because allow_samd_write is false");
    return;
  }
  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    this->publish_firmware_status_("test write failed: SWD pins missing");
    return;
  }

  bool core_halted = false;
  auto fail = [&](const std::string &error) {
    if (core_halted) {
      if (!this->resume_core_()) {
        ESP_LOGW(TAG, "Failed to resume SAMD09 core after test write error: %s", this->last_error_.c_str());
      }
      core_halted = false;
    }
    this->release_pins_();
    this->publish_status_("test write failed: " + error);
    this->publish_firmware_status_("test write failed: " + error);
    ESP_LOGW(TAG, "SAMD09 flash write test failed: %s", error.c_str());
  };

  this->prepare_pins_();
  this->begin_swd_session_();

  uint32_t swd_idcode = 0;
  if (!this->swd_initialize_(&swd_idcode)) {
    this->finish_swd_session_();
    fail(this->last_error_);
    return;
  }
  if (this->swd_idcode_sensor_ != nullptr) {
    this->swd_idcode_sensor_->publish_state(hex32_(swd_idcode));
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    fail(this->last_error_);
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    fail(this->last_error_);
    return;
  }

  uint32_t nvm_param = 0;
  uint32_t page_size = 0;
  uint32_t page_count = 0;
  uint32_t flash_size = 0;
  if (!this->read_flash_geometry_(&nvm_param, &page_size, &page_count, &flash_size)) {
    fail("geometry read failed: " + this->last_error_);
    return;
  }
  const uint32_t row_size = page_size * NVM_PAGES_PER_ROW;
  if (page_size == 0 || (page_size % 4U) != 0 || page_size > BACKUP_IO_BLOCK_SIZE || row_size == 0 ||
      flash_size < row_size || (flash_size % row_size) != 0) {
    fail("unsupported flash geometry");
    return;
  }

  if (!this->halt_core_()) {
    fail("halt failed: " + this->last_error_);
    return;
  }
  core_halted = true;

  bool row_erased = false;
  const uint32_t last_row_address = FLASH_START + flash_size - row_size;
  uint32_t row_address = last_row_address;
  if (!this->flash_row_erased_(last_row_address, row_size, &row_erased)) {
    fail("last test row read failed at " + hex32_(last_row_address) + ": " + this->last_error_);
    return;
  }
  if (!row_erased) {
    if (flash_size < (2U * row_size)) {
      fail("last test row is not erased and no previous row exists");
      return;
    }
    const uint32_t previous_row_address = FLASH_START + flash_size - (2U * row_size);
    if (!this->flash_row_erased_(previous_row_address, row_size, &row_erased)) {
      fail("previous test row read failed at " + hex32_(previous_row_address) + ": " + this->last_error_);
      return;
    }
    if (!row_erased) {
      fail("last two test rows are not erased");
      return;
    }
    row_address = previous_row_address;
    ESP_LOGI(TAG, "SAMD09 flash write test using previous row because last row is not erased");
  }

  if (!this->nvm_clear_errors_()) {
    fail("NVM error clear failed: " + this->last_error_);
    return;
  }
  if (!this->mem_write32_(NVMCTRL_CTRLB, 0x00000082UL)) {
    fail("NVM manual write setup failed: " + this->last_error_);
    return;
  }
  if (!this->nvm_wait_ready_()) {
    fail("NVM not ready: " + this->last_error_);
    return;
  }

  this->publish_firmware_status_("test write writing " + hex32_(row_address));
  if (!this->test_write_flash_page_(row_address, page_size)) {
    fail("test page write failed at " + hex32_(row_address) + ": " + this->last_error_);
    return;
  }

  this->publish_firmware_status_("test write erasing " + hex32_(row_address));
  if (!this->erase_flash_row_(row_address)) {
    fail("test row cleanup erase failed at " + hex32_(row_address) + ": " + this->last_error_);
    return;
  }
  if (!this->flash_row_erased_(row_address, row_size, &row_erased)) {
    fail("test row cleanup read failed at " + hex32_(row_address) + ": " + this->last_error_);
    return;
  }
  if (!row_erased) {
    fail("test row cleanup verify failed at " + hex32_(row_address));
    return;
  }

  if (!this->resume_core_()) {
    ESP_LOGW(TAG, "Failed to resume SAMD09 core after flash write test: %s", this->last_error_.c_str());
  }
  core_halted = false;
  this->release_pins_();

  this->publish_status_("test write complete");
  this->publish_firmware_status_("test write complete");
  ESP_LOGI(TAG, "SAMD09 flash write test complete: row=%s, page_size=%" PRIu32 ", row_size=%" PRIu32,
           hex32_(row_address).c_str(), page_size, row_size);
}

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
  if (this->dump_active_) {
    ESP_LOGW(TAG, "SAMD09 flash dump is running; firmware %s ignored", requested_name);
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
  this->publish_firmware_action_(std::string(requested_name) + " checking");
  this->publish_firmware_status_(std::string(requested_name) + " checking prerequisites");

  if (this->swdio_pin_ == nullptr || this->swclk_pin_ == nullptr) {
    this->set_error_("SWD pins are not configured");
    this->publish_firmware_update_available_(false);
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_action_("blocked: SWD pins missing");
    this->publish_firmware_status_(std::string(requested_name) + " failed: SWD pins missing");
    return;
  }

  bool core_halted = false;
  auto release_after_check = [&]() {
    if (core_halted) {
      const std::string prior_error = this->last_error_;
      if (!this->resume_core_()) {
        ESP_LOGW(TAG, "Failed to resume SAMD09 core after firmware %s check: %s", requested_name,
                 this->last_error_.c_str());
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
    this->publish_firmware_update_available_(false);
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_action_("failed: " + this->last_error_);
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware %s check failed: %s", requested_name, this->last_error_.c_str());
    return;
  }
  if (this->swd_idcode_sensor_ != nullptr) {
    this->swd_idcode_sensor_->publish_state(hex32_(swd_idcode));
  }

  if (!this->power_up_debug_()) {
    this->finish_swd_session_();
    release_after_check();
    this->publish_firmware_update_available_(false);
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_action_("failed: " + this->last_error_);
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware %s check failed: %s", requested_name, this->last_error_.c_str());
    return;
  }
  this->finish_swd_session_();

  if (!this->verify_mem_ap_()) {
    release_after_check();
    this->publish_firmware_update_available_(false);
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_action_("failed: " + this->last_error_);
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware %s check failed: %s", requested_name, this->last_error_.c_str());
    return;
  }

  if (!this->halt_core_()) {
    release_after_check();
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware %s failed while halting core: %s", requested_name, this->last_error_.c_str());
    return;
  }
  core_halted = true;

  FirmwareInfo current = this->detected_firmware_info_valid_ ? this->detected_firmware_info_ : FirmwareInfo{};
  uint32_t nvm_param = 0;
  uint32_t page_size = 0;
  uint32_t page_count = 0;
  uint32_t flash_size = 0;
  if (!this->read_flash_geometry_(&nvm_param, &page_size, &page_count, &flash_size)) {
    release_after_check();
    this->publish_firmware_update_available_(false);
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_action_("failed: " + this->last_error_);
    this->publish_firmware_status_(std::string(requested_name) + " failed: " + this->last_error_);
    ESP_LOGW(TAG, "SAMD09 firmware %s check failed: %s", requested_name, this->last_error_.c_str());
    return;
  }
  current.flash_size = flash_size;
  if (current.image_size == 0) {
    current.image_size = flash_size;
  }
  if (current.i2c_frame_length == 0) {
    current.i2c_frame_length = STOCK_I2C_FRAME_SIZE;
  }
  current.page_size = page_size;
  current.page_count = page_count;
  current.nvm_param = nvm_param;

  BackupHeader backup_header{};
  std::string backup_error;
  const bool backup_valid = this->read_valid_backup_(&backup_header, &backup_error);

  std::string action_reason;
  FirmwareAction action = FirmwareAction::UNKNOWN;
  if (restore_requested) {
    action = backup_valid ? FirmwareAction::RESTORE_STOCK : FirmwareAction::UNKNOWN;
    action_reason = backup_valid ? "stock backup is available" : "valid stock backup required";
  } else if (this->require_backup_before_install_ && !backup_valid) {
    action = FirmwareAction::BACKUP_STOCK;
    action_reason = "stock backup required before update";
  } else {
    action = FirmwareAction::UPDATE_MANAGED;
    action_reason = "firmware update requested";
  }
  this->publish_detected_firmware_action_(action, action_reason);
  if (restore_requested && backup_valid) {
    this->publish_firmware_restore_available_(true);
  }

  if (!restore_requested && action == FirmwareAction::BACKUP_STOCK) {
    release_after_check();
    this->publish_firmware_action_("backup required before update");
    this->publish_firmware_status_("backup required before update; starting backup");
    this->backup_firmware();
    return;
  }

  if (!restore_requested && action == FirmwareAction::UNKNOWN) {
    release_after_check();
    this->publish_firmware_update_available_(false);
    this->publish_firmware_action_("update blocked: " + action_reason);
    this->publish_firmware_status_("update blocked: " + action_reason);
    ESP_LOGW(TAG, "SAMD09 firmware update blocked: %s", action_reason.c_str());
    return;
  }

  if (!restore_requested && action != FirmwareAction::UPDATE_MANAGED) {
    release_after_check();
    this->publish_firmware_status_("update not needed: " + action_reason);
    ESP_LOGI(TAG, "SAMD09 firmware update not needed: %s", action_reason.c_str());
    return;
  }

  if (restore_requested && !backup_valid) {
    release_after_check();
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_action_("restore unavailable");
    this->publish_firmware_status_("restore unavailable: " + action_reason);
    ESP_LOGI(TAG, "SAMD09 stock restore unavailable: %s", action_reason.c_str());
    return;
  }

  const FirmwareAction selected_action = restore_requested ? FirmwareAction::RESTORE_STOCK : action;
  const std::string selected_reason = restore_requested ? "stock backup is available" : action_reason;

  if (!this->allow_samd_write_) {
    release_after_check();
    this->publish_firmware_action_("blocked: " + selected_reason + " (allow_samd_write is false)");
    this->publish_firmware_status_(std::string(requested_name) + " blocked: allow_samd_write is false");
    ESP_LOGW(TAG, "SAMD09 firmware %s blocked because allow_samd_write is false", requested_name);
    return;
  }

  if (selected_action == FirmwareAction::UPDATE_MANAGED && !this->bundled_firmware_available_()) {
    release_after_check();
    const char *current_kind = current.kind == FirmwareKind::MANAGED
                                   ? "managed"
                                   : (current.kind == FirmwareKind::STOCK ? "stock" : "unknown");
    this->publish_firmware_status_(
        str_sprintf("update unavailable: %s firmware needs v%s, but no bundled image is compiled in", current_kind,
                    format_firmware_version_(this->required_firmware_version_).c_str()));
    ESP_LOGW(TAG,
             "SAMD09 managed firmware update is needed but no bundled firmware image is compiled in: current_kind=%s, "
             "current_version=%" PRIu32 ", required=%" PRIu32,
             current_kind, current.version, this->required_firmware_version_);
    return;
  }

  if (selected_action == FirmwareAction::UPDATE_MANAGED &&
      this->bundled_firmware_version_() < this->required_firmware_version_) {
    release_after_check();
    this->publish_firmware_status_(str_sprintf("update unavailable: bundle v%s is older than required v%s",
                                               format_firmware_version_(this->bundled_firmware_version_()).c_str(),
                                               format_firmware_version_(this->required_firmware_version_).c_str()));
    ESP_LOGW(TAG, "SAMD09 bundled firmware is older than required version");
    return;
  }

  if (selected_action == FirmwareAction::UPDATE_MANAGED && this->hardware_id_ != 0 &&
      this->bundled_firmware_hardware_id_() != this->hardware_id_) {
    release_after_check();
    this->publish_firmware_status_(str_sprintf("update blocked: bundled image hardware id %" PRIu32
                                               " != configured hardware id %u",
                                               this->bundled_firmware_hardware_id_(),
                                               static_cast<unsigned>(this->hardware_id_)));
    ESP_LOGW(TAG, "SAMD09 firmware update blocked by bundled image hardware mismatch");
    return;
  }

  if (selected_action == FirmwareAction::RESTORE_STOCK && !backup_valid) {
    release_after_check();
    this->publish_firmware_restore_available_(false);
    this->publish_firmware_status_("restore blocked: valid stock backup required (" + backup_error + ")");
    ESP_LOGW(TAG, "SAMD09 stock restore blocked: valid backup required (%s)", backup_error.c_str());
    return;
  }

  const uint32_t source_size =
      selected_action == FirmwareAction::RESTORE_STOCK ? backup_header.flash_size : this->bundled_firmware_size_();
  if (source_size != current.flash_size) {
    release_after_check();
    this->publish_firmware_status_(str_sprintf("%s blocked: image size %" PRIu32 " != flash size %" PRIu32,
                                               requested_name,
                                               source_size, current.flash_size));
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

  if (selected_action == FirmwareAction::UPDATE_MANAGED && this->require_backup_before_install_ && !backup_valid) {
    release_after_check();
    this->publish_firmware_status_("update blocked: valid stock backup required (" + backup_error + ")");
    ESP_LOGW(TAG, "SAMD09 firmware update blocked: valid backup required (%s)", backup_error.c_str());
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
  if (backup_valid) {
    this->install_backup_header_ = backup_header;
  }
  this->install_flash_size_ = current.flash_size;
  this->install_page_size_ = current.page_size;
  this->install_row_size_ = current.page_size * NVM_PAGES_PER_ROW;
  this->install_next_offset_ = 0;
  this->install_stage_ = InstallStage::FLASH_PAGES;
  this->install_active_ = true;
  this->publish_status_(std::string(this->install_action_name_()) + " SAMD firmware");
  this->publish_firmware_status_(str_sprintf("%s flashing 0/%" PRIu32, this->install_action_name_(),
                                             this->install_flash_size_));
  ESP_LOGI(TAG,
           "SAMD09 firmware %s job started: current_kind=%s, current_version=%" PRIu32
           ", target_hardware=%" PRIu32 ", target_version=%" PRIu32 ", source_size=%" PRIu32
           ", page_size=%" PRIu32 ", row_size=%" PRIu32,
           this->install_action_name_(),
           current.kind == FirmwareKind::MANAGED ? "managed"
                                                  : (current.kind == FirmwareKind::STOCK ? "stock" : "unknown"),
           current.version,
           selected_action == FirmwareAction::UPDATE_MANAGED ? this->bundled_firmware_hardware_id_() : 0,
           selected_action == FirmwareAction::UPDATE_MANAGED ? this->bundled_firmware_version_() : 0, source_size,
           this->install_page_size_,
           this->install_row_size_);
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
  delay(this->reset_hold_time_ms_);
}

void EmporiaVueComponent::deassert_reset_() {
  this->reset_pin_->digital_write(true);
  delay(this->reset_release_time_ms_);
}

bool EmporiaVueComponent::connect_under_reset_active_() const {
  return this->connect_under_reset_ && this->reset_pin_ != nullptr;
}

void EmporiaVueComponent::begin_swd_session_() {
  if (this->connect_under_reset_active_()) {
    ESP_LOGI(TAG, "Asserting SAMD09 reset for connect-under-reset");
    this->assert_reset_();
    return;
  }
  if (this->connect_under_reset_ && this->reset_pin_ == nullptr) {
    ESP_LOGW(TAG, "connect_under_reset is enabled but reset_pin is not configured");
  }
  this->reset_target_();
}

void EmporiaVueComponent::finish_swd_session_() {
  if (this->connect_under_reset_active_()) {
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

bool EmporiaVueComponent::probe_idcode_(const char *sequence_name, uint8_t swj_select_bits, bool sample_before_clock,
                                        uint32_t *idcode, uint8_t *ack) {
  ESP_LOGI(TAG, "Trying SAMD09 %s IDCODE probe", sequence_name);
  this->last_error_.clear();
  this->sample_before_clock_ = sample_before_clock;
  this->swd_enter_debug_(swj_select_bits);
  if (this->transfer_(false, true, DP_IDCODE, 0, idcode, ack)) {
    return true;
  }
  ESP_LOGI(TAG, "SAMD09 %s IDCODE probe ACK=%s", sequence_name, hex8_(*ack).c_str());
  return false;
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
    ESP_LOGI(TAG, "Trying SAMD09 %s initialization", variant.name);
    this->last_error_.clear();
    this->sample_before_clock_ = variant.sample_before_clock;
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

void EmporiaVueComponent::publish_status_(const std::string &status) {
  if (this->status_sensor_ != nullptr) {
    this->status_sensor_->publish_state(status);
  }
}

void EmporiaVueComponent::publish_firmware_status_(const std::string &status) {
  if (this->firmware_status_sensor_ != nullptr) {
    this->firmware_status_sensor_->publish_state(status);
  }
}

void EmporiaVueComponent::publish_firmware_action_(const std::string &action) {
  if (this->firmware_action_sensor_ != nullptr) {
    this->firmware_action_sensor_->publish_state(action);
  }
}

void EmporiaVueComponent::publish_firmware_version_(const FirmwareInfo &info) {
  if (this->firmware_version_sensor_ == nullptr) {
    return;
  }
  switch (info.kind) {
    case FirmwareKind::MANAGED:
      this->firmware_version_sensor_->publish_state(
          str_sprintf("managed hw=%u v%s (i2c %u bytes)", static_cast<unsigned>(info.hardware_id),
                      format_firmware_version_(info.version).c_str(),
                      static_cast<unsigned>(info.i2c_frame_length)));
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

void EmporiaVueComponent::publish_firmware_update_available_(bool available) {
  if (this->firmware_update_available_sensor_ != nullptr) {
    this->firmware_update_available_sensor_->publish_state(available);
  }
}

void EmporiaVueComponent::publish_firmware_restore_available_(bool available) {
  if (this->firmware_restore_available_sensor_ != nullptr) {
    this->firmware_restore_available_sensor_->publish_state(available);
  }
}

void EmporiaVueComponent::publish_read_allowed_(bool value) {
  if (this->read_allowed_sensor_ != nullptr) {
    this->read_allowed_sensor_->publish_state(value);
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
  ESP_LOGD(TAG, "Halting SAMD09 core for flash dump block");
  return this->mem_write32_(DHCSR, DHCSR_DBGKEY | DHCSR_C_DEBUGEN | DHCSR_C_HALT);
}

bool EmporiaVueComponent::resume_core_() {
  ESP_LOGD(TAG, "Resuming SAMD09 core after flash dump block");
  return this->mem_write32_(DHCSR, DHCSR_DBGKEY | DHCSR_C_DEBUGEN);
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

bool EmporiaVueComponent::dump_flash_block_(uint32_t address, uint16_t length, std::string *hex_data) {
  hex_data->clear();
  hex_data->reserve(uint32_t(length) * 2U);

  uint16_t offset = 0;
  while (offset < length) {
    const uint32_t current_address = address + offset;
    const uint16_t remaining = length - offset;
    if ((current_address & 0x03U) == 0 && remaining >= 4) {
      uint32_t word = 0;
      if (!this->mem_read32_(current_address, &word)) {
        return false;
      }
      append_hex_byte_(hex_data, static_cast<uint8_t>(word & 0xFFU));
      append_hex_byte_(hex_data, static_cast<uint8_t>((word >> 8) & 0xFFU));
      append_hex_byte_(hex_data, static_cast<uint8_t>((word >> 16) & 0xFFU));
      append_hex_byte_(hex_data, static_cast<uint8_t>((word >> 24) & 0xFFU));
      offset += 4;
    } else {
      uint8_t byte = 0;
      if (!this->mem_read8_(current_address, &byte)) {
        return false;
      }
      append_hex_byte_(hex_data, byte);
      offset++;
    }
  }
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

EmporiaVueComponent::ManagedI2CInfoResult EmporiaVueComponent::query_managed_i2c_info_(
    ManagedI2CInfo *managed_info) {
  ManagedI2CInfo candidate{};
  const uint8_t command = MANAGED_I2C_INFO_COMMAND;
  const i2c::ErrorCode error =
      this->write_read(&command, 1, reinterpret_cast<uint8_t *>(&candidate), sizeof(candidate));
  if (error != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "SAMD09 managed I2C info query failed: i2c error %u", static_cast<unsigned>(error));
    return ManagedI2CInfoResult::I2C_ERROR;
  }
  if (!this->validate_managed_i2c_info_(candidate)) {
    ESP_LOGD(TAG, "SAMD09 managed I2C info query returned no valid managed firmware response");
    return ManagedI2CInfoResult::INVALID_RESPONSE;
  }

  *managed_info = candidate;
  ESP_LOGD(TAG, "SAMD09 managed I2C info: hardware_id=%u, version=%" PRIu32 " (v%s), i2c_frame_length=%u",
           static_cast<unsigned>(candidate.hardware_id), candidate.firmware_version,
           format_firmware_version_(candidate.firmware_version).c_str(),
           static_cast<unsigned>(candidate.i2c_frame_length));
  return ManagedI2CInfoResult::VALID_RESPONSE;
}

bool EmporiaVueComponent::read_managed_i2c_info_(ManagedI2CInfo *managed_info) {
  return this->query_managed_i2c_info_(managed_info) == ManagedI2CInfoResult::VALID_RESPONSE;
}

bool EmporiaVueComponent::validate_managed_i2c_info_(const ManagedI2CInfo &managed_info) const {
  const uint32_t expected_crc =
      crc32_(reinterpret_cast<const uint8_t *>(&managed_info), offsetof(ManagedI2CInfo, crc32));
  if (expected_crc != managed_info.crc32) {
    return false;
  }
  if (managed_info.hardware_id != 2 && managed_info.hardware_id != 3) {
    return false;
  }
  if (managed_info.firmware_version == 0) {
    return false;
  }
  return managed_info.i2c_frame_length == STOCK_I2C_FRAME_SIZE;
}

void EmporiaVueComponent::publish_initial_firmware_detection_() {
  if (this->backup_active_ || this->install_active_ || this->dump_active_) {
    return;
  }

  ManagedI2CInfo managed_i2c_info{};
  FirmwareInfo info{};
  const ManagedI2CInfoResult result = this->query_managed_i2c_info_(&managed_i2c_info);
  if (result == ManagedI2CInfoResult::VALID_RESPONSE) {
    info.kind = FirmwareKind::MANAGED;
    info.hardware_id = managed_i2c_info.hardware_id;
    info.version = managed_i2c_info.firmware_version;
    info.i2c_frame_length = managed_i2c_info.i2c_frame_length;
    info.detected_by_i2c = true;
    this->detected_firmware_info_ = info;
    this->detected_firmware_info_valid_ = true;
    this->publish_firmware_version_(info);
    this->publish_firmware_status_("managed firmware detected");
    return;
  }

  if (result == ManagedI2CInfoResult::INVALID_RESPONSE) {
    info.kind = FirmwareKind::STOCK;
    info.i2c_frame_length = STOCK_I2C_FRAME_SIZE;
    this->detected_firmware_info_ = info;
    this->detected_firmware_info_valid_ = true;
    this->publish_firmware_version_(info);
    this->publish_firmware_status_("stock firmware detected");
    return;
  }

  this->detected_firmware_info_ = info;
  this->detected_firmware_info_valid_ = false;
  this->publish_firmware_version_(info);
  this->publish_firmware_status_("firmware detection unavailable: I2C query failed");
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

  ManagedFirmwareInfo candidate{};
  const uint32_t info_address = FLASH_START + flash_size - sizeof(ManagedFirmwareInfo);
  if (!this->read_flash_bytes_(info_address, sizeof(candidate), reinterpret_cast<uint8_t *>(&candidate))) {
    return false;
  }

  if (candidate.magic != MANAGED_INFO_MAGIC || candidate.format_version != MANAGED_INFO_FORMAT_VERSION ||
      std::memcmp(candidate.marker, MANAGED_MARKER, MANAGED_MARKER_LENGTH) != 0) {
    return true;
  }

  *managed_info = candidate;
  *found = true;
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
  info->detected_by_i2c = false;
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
    info->version = managed_info.firmware_version;
    info->image_size = managed_info.image_size;
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

bool EmporiaVueComponent::backup_partition_valid_(std::string *error) {
  return this->read_valid_backup_(nullptr, error);
}

EmporiaVueComponent::FirmwareAction EmporiaVueComponent::determine_firmware_action_(
    const FirmwareInfo &current, const BackupHeader *backup_header, std::string *reason) const {
  const bool backup_valid = backup_header != nullptr;
  if (current.kind == FirmwareKind::STOCK) {
    if (this->require_backup_before_install_ && !backup_valid) {
      if (reason != nullptr) {
        *reason = "stock firmware detected; backup required before update";
      }
      return FirmwareAction::BACKUP_STOCK;
    }
    if (reason != nullptr) {
      *reason = str_sprintf("stock firmware detected; update to managed hw=%" PRIu32 " v%s",
                            this->bundled_firmware_hardware_id_(),
                            format_firmware_version_(this->required_firmware_version_).c_str());
    }
    return FirmwareAction::UPDATE_MANAGED;
  }

  if (current.kind == FirmwareKind::MANAGED && this->hardware_id_ != 0 &&
      current.hardware_id != this->hardware_id_) {
    if (reason != nullptr) {
      *reason = str_sprintf("managed hardware id %u does not match configured hardware id %u",
                            static_cast<unsigned>(current.hardware_id), static_cast<unsigned>(this->hardware_id_));
    }
    return FirmwareAction::UPDATE_MANAGED;
  }

  if (current.kind == FirmwareKind::MANAGED && current.version < this->required_firmware_version_) {
    if (reason != nullptr) {
      *reason = str_sprintf("managed hw=%u v%s is older than required v%s",
                            static_cast<unsigned>(current.hardware_id),
                            format_firmware_version_(current.version).c_str(),
                            format_firmware_version_(this->required_firmware_version_).c_str());
    }
    return FirmwareAction::UPDATE_MANAGED;
  }

  if (current.kind == FirmwareKind::MANAGED && backup_valid) {
    if (reason != nullptr) {
      *reason = str_sprintf("managed hw=%u v%s is current; stock backup is available",
                            static_cast<unsigned>(current.hardware_id),
                            format_firmware_version_(current.version).c_str());
    }
    return FirmwareAction::RESTORE_STOCK;
  }

  if (current.kind == FirmwareKind::MANAGED) {
    if (reason != nullptr) {
      *reason = str_sprintf("up to date: managed hw=%u v%s", static_cast<unsigned>(current.hardware_id),
                            format_firmware_version_(current.version).c_str());
    }
    return FirmwareAction::NONE;
  }

  if (reason != nullptr) {
    *reason = "unknown firmware state";
  }
  return FirmwareAction::UNKNOWN;
}

void EmporiaVueComponent::publish_detected_firmware_action_(FirmwareAction action, const std::string &reason) {
  this->publish_firmware_update_available_(action == FirmwareAction::UPDATE_MANAGED ||
                                           action == FirmwareAction::BACKUP_STOCK);
  this->publish_firmware_restore_available_(action == FirmwareAction::RESTORE_STOCK);

  switch (action) {
    case FirmwareAction::BACKUP_STOCK:
      this->publish_firmware_action_("backup required before update: " + reason);
      break;
    case FirmwareAction::UPDATE_MANAGED:
      this->publish_firmware_action_("update available: " + reason);
      break;
    case FirmwareAction::RESTORE_STOCK:
      this->publish_firmware_action_("restore stock available: " + reason);
      break;
    case FirmwareAction::NONE:
      this->publish_firmware_action_(reason);
      break;
    case FirmwareAction::UNKNOWN:
    default:
      this->publish_firmware_action_("unknown: " + reason);
      break;
  }
}

bool EmporiaVueComponent::bundled_firmware_available_() const { return BUNDLED_SAMD_FIRMWARE_SIZE > 0; }

uint32_t EmporiaVueComponent::bundled_firmware_hardware_id_() const { return BUNDLED_SAMD_FIRMWARE_HARDWARE_ID; }

uint32_t EmporiaVueComponent::bundled_firmware_version_() const { return BUNDLED_SAMD_FIRMWARE_VERSION; }

uint32_t EmporiaVueComponent::bundled_firmware_size_() const { return BUNDLED_SAMD_FIRMWARE_SIZE; }

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

bool EmporiaVueComponent::write_raw_flash_page_(uint32_t address, const uint8_t *data, uint32_t length) {
  if (length == 0 || length > BACKUP_IO_BLOCK_SIZE || (length % 4U) != 0) {
    this->set_error_("raw page write length unsupported");
    return false;
  }
  if (!this->nvm_clear_errors_()) {
    return false;
  }
  if (!this->nvm_command_(NVM_CMD_PAGE_BUFFER_CLEAR)) {
    return false;
  }

  for (uint32_t i = 0; i < length; i += 4) {
    const uint32_t word = uint32_t(data[i]) | (uint32_t(data[i + 1]) << 8) |
                          (uint32_t(data[i + 2]) << 16) | (uint32_t(data[i + 3]) << 24);
    if (!this->mem_write32_(address + i, word)) {
      return false;
    }
  }

  if (!this->mem_write32_(NVMCTRL_ADDR, (address - FLASH_START) >> 1)) {
    return false;
  }
  return this->nvm_command_(NVM_CMD_WRITE_PAGE);
}

bool EmporiaVueComponent::test_write_flash_page_(uint32_t address, uint32_t page_size) {
  if (page_size == 0 || page_size > BACKUP_IO_BLOCK_SIZE || (page_size % 4U) != 0) {
    this->set_error_("test write page size unsupported");
    return false;
  }

  uint8_t expected[BACKUP_IO_BLOCK_SIZE]{};
  uint8_t actual[BACKUP_IO_BLOCK_SIZE]{};
  for (uint32_t i = 0; i < page_size; i++) {
    const uint8_t address_byte = static_cast<uint8_t>((address >> ((i & 0x03U) * 8U)) & 0xFFU);
    expected[i] = static_cast<uint8_t>(0xA5U ^ ((i * 17U) & 0xFFU) ^ address_byte);
  }

  if (!this->write_raw_flash_page_(address, expected, page_size)) {
    return false;
  }
  if (!this->read_flash_bytes_(address, static_cast<uint16_t>(page_size), actual)) {
    return false;
  }
  for (uint32_t i = 0; i < page_size; i++) {
    if (actual[i] != expected[i]) {
      this->set_error_(str_sprintf("test write mismatch at %s: read=%s expected=%s", hex32_(address + i).c_str(),
                                   hex8_(actual[i]).c_str(), hex8_(expected[i]).c_str()));
      return false;
    }
  }
  return true;
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
    case FirmwareAction::BACKUP_STOCK:
      return "backup";
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
    this->publish_firmware_status_(str_sprintf("%s erasing %" PRIu32 "/%" PRIu32, this->install_action_name_(),
                                               offset, this->install_flash_size_));
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
  if ((this->install_next_offset_ % 1024U) == 0 || this->install_next_offset_ >= this->install_flash_size_) {
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
        info.version != this->bundled_firmware_version_()) {
      this->fail_install_("final managed firmware marker verification failed");
      return;
    }
  } else if (completed_action == FirmwareAction::RESTORE_STOCK) {
    if (!info_ok || info.kind != FirmwareKind::STOCK) {
      this->fail_install_("final stock firmware verification failed");
      return;
    }
  } else {
    this->fail_install_("internal firmware action state error");
    return;
  }

  if (this->install_core_halted_ && !this->resume_core_()) {
    ESP_LOGW(TAG, "Failed to resume SAMD09 core after firmware %s: %s", action_name.c_str(),
             this->last_error_.c_str());
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

  BackupHeader backup_header{};
  std::string backup_error;
  const bool backup_valid = this->read_valid_backup_(&backup_header, &backup_error);
  if (completed_action == FirmwareAction::UPDATE_MANAGED) {
    this->publish_firmware_update_available_(false);
    this->publish_firmware_restore_available_(backup_valid);
    this->publish_status_("update complete");
    this->publish_firmware_action_(str_sprintf("up to date: managed hw=%" PRIu32 " v%s",
                                               this->bundled_firmware_hardware_id_(),
                                               format_firmware_version_(this->bundled_firmware_version_()).c_str()));
    this->publish_firmware_status_(
        str_sprintf("update complete: managed hw=%" PRIu32 " v%s",
                    this->bundled_firmware_hardware_id_(),
                    format_firmware_version_(this->bundled_firmware_version_()).c_str()));
    ESP_LOGI(TAG, "SAMD09 managed firmware update complete: hardware_id=%" PRIu32 ", version=%" PRIu32
                  " (v%s)"
                  ", size=%" PRIu32 ", sha256=%s",
             this->bundled_firmware_hardware_id_(), this->bundled_firmware_version_(),
             format_firmware_version_(this->bundled_firmware_version_()).c_str(), this->bundled_firmware_size_(),
             sha256_hex_(BUNDLED_SAMD_FIRMWARE_SHA256).c_str());
  } else {
    this->publish_firmware_update_available_(this->bundled_firmware_available_() &&
                                             this->bundled_firmware_version_() >= this->required_firmware_version_);
    this->publish_firmware_restore_available_(false);
    this->publish_status_("restore complete");
    this->publish_firmware_action_("stock firmware restored");
    this->publish_firmware_status_("restore complete: stock firmware");
    ESP_LOGI(TAG, "SAMD09 stock firmware restore complete: size=%" PRIu32, this->install_flash_size_);
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
