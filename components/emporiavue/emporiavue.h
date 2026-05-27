#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <cstdint>
#include <string>

namespace esphome {
namespace emporiavue {

class EmporiaVueComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_swdio_pin(InternalGPIOPin *pin) { this->swdio_pin_ = pin; }
  void set_swclk_pin(InternalGPIOPin *pin) { this->swclk_pin_ = pin; }
  void set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }
  void set_reset_before_read(bool reset_before_read) { this->reset_before_read_ = reset_before_read; }
  void set_connect_under_reset(bool connect_under_reset) { this->connect_under_reset_ = connect_under_reset; }
  void set_reset_hold_time(uint32_t reset_hold_time) { this->reset_hold_time_ms_ = reset_hold_time; }
  void set_reset_release_time(uint32_t reset_release_time) { this->reset_release_time_ms_ = reset_release_time; }
  void set_clock_delay_us(uint8_t clock_delay_us) { this->clock_delay_us_ = clock_delay_us; }
  void set_retry_count(uint8_t retry_count) { this->retry_count_ = retry_count; }
  void set_init_pins_on_boot(bool init_pins_on_boot) { this->init_pins_on_boot_ = init_pins_on_boot; }
  void set_dump_start_address(uint32_t dump_start_address) { this->dump_start_address_ = dump_start_address; }
  void set_dump_block_size(uint16_t dump_block_size) { this->dump_block_size_ = dump_block_size; }
  void set_dump_block_count(uint16_t dump_block_count) { this->dump_block_count_ = dump_block_count; }
  void set_dump_halt_core(bool dump_halt_core) { this->dump_halt_core_ = dump_halt_core; }
  void set_dump_resume_between_blocks(bool dump_resume_between_blocks) {
    this->dump_resume_between_blocks_ = dump_resume_between_blocks;
  }

  void set_swd_idcode_sensor(text_sensor::TextSensor *sensor) { this->swd_idcode_sensor_ = sensor; }
  void set_dsu_did_sensor(text_sensor::TextSensor *sensor) { this->dsu_did_sensor_ = sensor; }
  void set_status_sensor(text_sensor::TextSensor *sensor) { this->status_sensor_ = sensor; }
  void set_read_allowed_sensor(binary_sensor::BinarySensor *sensor) { this->read_allowed_sensor_ = sensor; }

  void read_samd();
  void probe_swd();
  void dump_flash();

 protected:
  static constexpr uint8_t DP_ABORT = 0x00;
  static constexpr uint8_t DP_IDCODE = 0x00;
  static constexpr uint8_t DP_CTRL_STAT = 0x04;
  static constexpr uint8_t DP_SELECT = 0x08;
  static constexpr uint8_t DP_RDBUFF = 0x0C;
  static constexpr uint8_t AP_CSW = 0x00;
  static constexpr uint8_t AP_TAR = 0x04;
  static constexpr uint8_t AP_DRW = 0x0C;
  static constexpr uint8_t AP_IDR = 0xFC;

  static constexpr uint8_t SWD_ACK_OK = 0b001;
  static constexpr uint8_t SWD_ACK_WAIT = 0b010;
  static constexpr uint8_t SWD_ACK_FAULT = 0b100;

  static constexpr uint32_t MEM_AP_CSW_BASE = 0x23000040UL;
  static constexpr uint32_t DP_POWER_REQUEST = 0x50000000UL;
  static constexpr uint32_t DP_POWER_ACK = 0xF0000000UL;

  static constexpr uint32_t DSU_EXTERNAL_BASE = 0x41002100UL;
  static constexpr uint32_t DSU_STATUSB = DSU_EXTERNAL_BASE + 0x02UL;
  static constexpr uint32_t DSU_DID = DSU_EXTERNAL_BASE + 0x18UL;
  static constexpr uint32_t NVMCTRL_STATUS = 0x41004018UL;
  static constexpr uint32_t FLASH_START = 0x00000000UL;
  static constexpr uint32_t DHCSR = 0xE000EDF0UL;
  static constexpr uint32_t DHCSR_DBGKEY = 0xA05F0000UL;
  static constexpr uint32_t DHCSR_C_DEBUGEN = 0x00000001UL;
  static constexpr uint32_t DHCSR_C_HALT = 0x00000002UL;

  enum MemSize : uint8_t {
    MEM_SIZE_BYTE = 0,
    MEM_SIZE_HALFWORD = 1,
    MEM_SIZE_WORD = 2,
  };

  void reset_target_();
  void assert_reset_();
  void deassert_reset_();
  bool connect_under_reset_active_() const;
  void begin_swd_session_();
  void finish_swd_session_();
  void swd_enter_debug_(uint8_t swj_select_bits);
  bool probe_idcode_(const char *sequence_name, uint8_t swj_select_bits, bool sample_before_clock, uint32_t *idcode,
                     uint8_t *ack);
  bool swd_initialize_(uint32_t *idcode);
  void prepare_pins_();
  void release_pins_();
  void set_error_(const std::string &error);
  void publish_status_(const std::string &status);
  void publish_read_allowed_(bool value);
  static std::string hex32_(uint32_t value);
  static std::string hex16_(uint16_t value);
  static std::string hex8_(uint8_t value);
  static void append_hex_byte_(std::string *output, uint8_t value);

  void clock_half_period_();
  void swclk_pulse_();
  void turnaround_(bool write);
  void write_bits_(uint32_t data, uint8_t bits);
  uint32_t read_bits_(uint8_t bits);
  static bool parity32_(uint32_t value);
  uint8_t make_request_(bool ap, bool read, uint8_t addr);

  bool transfer_(bool ap, bool read, uint8_t addr, uint32_t write_value, uint32_t *read_value, uint8_t *ack);
  bool dp_read_(uint8_t addr, uint32_t *value);
  bool dp_write_(uint8_t addr, uint32_t value);
  bool clear_sticky_errors_();
  bool select_ap_bank_(uint8_t bank);
  bool ap_read_(uint8_t addr, uint32_t *value);
  bool ap_write_(uint8_t addr, uint32_t value);
  bool mem_read_(uint32_t address, MemSize size, uint32_t *value);
  bool mem_read8_(uint32_t address, uint8_t *value);
  bool mem_read16_(uint32_t address, uint16_t *value);
  bool mem_read32_(uint32_t address, uint32_t *value);
  bool mem_write_(uint32_t address, MemSize size, uint32_t value);
  bool mem_write32_(uint32_t address, uint32_t value);
  bool halt_core_();
  bool resume_core_();
  bool dump_flash_block_(uint32_t address, uint16_t length, std::string *hex_data);
  bool power_up_debug_();
  bool verify_mem_ap_();

  InternalGPIOPin *swdio_pin_{nullptr};
  InternalGPIOPin *swclk_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
  text_sensor::TextSensor *swd_idcode_sensor_{nullptr};
  text_sensor::TextSensor *dsu_did_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  binary_sensor::BinarySensor *read_allowed_sensor_{nullptr};

  bool reset_before_read_{false};
  bool connect_under_reset_{false};
  uint32_t reset_hold_time_ms_{100};
  uint32_t reset_release_time_ms_{50};
  uint8_t clock_delay_us_{2};
  uint8_t retry_count_{40};
  uint32_t dump_start_address_{FLASH_START};
  uint16_t dump_block_size_{64};
  uint16_t dump_block_count_{5};
  bool dump_halt_core_{true};
  bool dump_resume_between_blocks_{true};
  bool dump_active_{false};
  uint16_t dump_next_block_{0};
  bool init_pins_on_boot_{false};
  bool pins_setup_{false};
  bool direction_write_{true};
  bool sample_before_clock_{false};
  bool selected_ap_valid_{false};
  uint8_t selected_ap_bank_{0};
  uint32_t cached_csw_{0xFFFFFFFFUL};
  std::string last_error_;
};

class EmporiaVueReadButton : public button::Button, public Parented<EmporiaVueComponent> {
 protected:
  void press_action() override { this->parent_->read_samd(); }
};

class EmporiaVueProbeButton : public button::Button, public Parented<EmporiaVueComponent> {
 protected:
  void press_action() override { this->parent_->probe_swd(); }
};

class EmporiaVueDumpFlashButton : public button::Button, public Parented<EmporiaVueComponent> {
 protected:
  void press_action() override { this->parent_->dump_flash(); }
};

}  // namespace emporiavue
}  // namespace esphome
