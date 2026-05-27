#include "emporiavue.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace emporiavue {

static const char *const TAG = "emporiavue";

void EmporiaVueComponent::setup() {
  this->perform_boot_reset_();
  if (this->init_pins_on_boot_) {
    this->prepare_pins_();
    this->release_pins_();
  }
}

void EmporiaVueComponent::loop() {
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
  ESP_LOGCONFIG(TAG, "  Init pins on boot: %s", YESNO(this->init_pins_on_boot_));
  LOG_TEXT_SENSOR("  ", "SWD IDCODE", this->swd_idcode_sensor_);
  LOG_TEXT_SENSOR("  ", "DSU DID", this->dsu_did_sensor_);
  LOG_TEXT_SENSOR("  ", "Status", this->status_sensor_);
  LOG_BINARY_SENSOR("  ", "Read allowed", this->read_allowed_sensor_);
}

void EmporiaVueComponent::read_samd() {
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
        this->set_error_(str_sprintf("parity error reading %s register 0x%02X", ap ? "AP" : "DP", addr));
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

}  // namespace emporiavue
}  // namespace esphome
