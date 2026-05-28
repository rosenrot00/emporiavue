#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <esp_partition.h>
#include <mbedtls/sha256.h>

#include <array>
#include <cstddef>
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
  void set_hardware_id(uint16_t hardware_id) { this->hardware_id_ = hardware_id; }
  void set_reset_before_read(bool reset_before_read) { this->reset_before_read_ = reset_before_read; }
  void set_reset_on_boot(bool reset_on_boot) { this->reset_on_boot_ = reset_on_boot; }
  void set_connect_under_reset(bool connect_under_reset) { this->connect_under_reset_ = connect_under_reset; }
  void set_reset_hold_time(uint32_t reset_hold_time) { this->reset_hold_time_ms_ = reset_hold_time; }
  void set_reset_release_time(uint32_t reset_release_time) { this->reset_release_time_ms_ = reset_release_time; }
  void set_clock_delay_us(uint8_t clock_delay_us) { this->clock_delay_us_ = clock_delay_us; }
  void set_retry_count(uint8_t retry_count) { this->retry_count_ = retry_count; }
  void set_init_pins_on_boot(bool init_pins_on_boot) { this->init_pins_on_boot_ = init_pins_on_boot; }
  void set_dump_start_address(uint32_t dump_start_address) { this->dump_start_address_ = dump_start_address; }
  void set_dump_block_size(uint16_t dump_block_size) { this->dump_block_size_ = dump_block_size; }
  void set_dump_block_count(uint32_t dump_block_count) { this->dump_block_count_ = dump_block_count; }
  void set_dump_full_flash(bool dump_full_flash) { this->dump_full_flash_ = dump_full_flash; }
  void set_dump_halt_core(bool dump_halt_core) { this->dump_halt_core_ = dump_halt_core; }
  void set_dump_resume_between_blocks(bool dump_resume_between_blocks) {
    this->dump_resume_between_blocks_ = dump_resume_between_blocks;
  }
  void set_backup_partition_name(const std::string &backup_partition_name) {
    this->backup_partition_name_ = backup_partition_name;
  }
  void set_required_firmware_version(uint32_t version) { this->required_firmware_version_ = version; }
  void set_allow_samd_write(bool allow_samd_write) { this->allow_samd_write_ = allow_samd_write; }
  void set_require_backup_before_install(bool require_backup) { this->require_backup_before_install_ = require_backup; }

  void set_swd_idcode_sensor(text_sensor::TextSensor *sensor) { this->swd_idcode_sensor_ = sensor; }
  void set_dsu_did_sensor(text_sensor::TextSensor *sensor) { this->dsu_did_sensor_ = sensor; }
  void set_status_sensor(text_sensor::TextSensor *sensor) { this->status_sensor_ = sensor; }
  void set_firmware_status_sensor(text_sensor::TextSensor *sensor) { this->firmware_status_sensor_ = sensor; }
  void set_firmware_action_sensor(text_sensor::TextSensor *sensor) { this->firmware_action_sensor_ = sensor; }
  void set_firmware_version_sensor(text_sensor::TextSensor *sensor) { this->firmware_version_sensor_ = sensor; }
  void set_read_allowed_sensor(binary_sensor::BinarySensor *sensor) { this->read_allowed_sensor_ = sensor; }
  void set_firmware_update_available_sensor(binary_sensor::BinarySensor *sensor) {
    this->firmware_update_available_sensor_ = sensor;
  }
  void set_firmware_restore_available_sensor(binary_sensor::BinarySensor *sensor) {
    this->firmware_restore_available_sensor_ = sensor;
  }

  void read_samd();
  void probe_swd();
  void dump_flash();
  void backup_firmware();
  void install_firmware();
  void restore_firmware();

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
  static constexpr uint32_t NVMCTRL_CTRLA = 0x41004000UL;
  static constexpr uint32_t NVMCTRL_CTRLB = 0x41004004UL;
  static constexpr uint32_t NVMCTRL_PARAM = 0x41004008UL;
  static constexpr uint32_t NVMCTRL_INTFLAG = 0x41004014UL;
  static constexpr uint32_t NVMCTRL_STATUS = 0x41004018UL;
  static constexpr uint32_t NVMCTRL_ADDR = 0x4100401CUL;
  static constexpr uint8_t NVM_CMD_ERASE_ROW = 0x02;
  static constexpr uint8_t NVM_CMD_WRITE_PAGE = 0x04;
  static constexpr uint8_t NVM_CMD_PAGE_BUFFER_CLEAR = 0x44;
  static constexpr uint16_t NVM_CMD_KEY = 0xA500;
  static constexpr uint8_t NVM_INTFLAG_READY = 0x01;
  static constexpr uint8_t NVM_INTFLAG_ERROR = 0x02;
  static constexpr uint16_t NVM_STATUS_PROGE = 0x0004;
  static constexpr uint16_t NVM_STATUS_LOCKE = 0x0008;
  static constexpr uint16_t NVM_STATUS_NVME = 0x0010;
  static constexpr uint16_t NVM_STATUS_ERROR_MASK = NVM_STATUS_PROGE | NVM_STATUS_LOCKE | NVM_STATUS_NVME;
  static constexpr uint32_t NVM_PAGES_PER_ROW = 4;
  static constexpr uint32_t FLASH_START = 0x00000000UL;
  static constexpr uint32_t DHCSR = 0xE000EDF0UL;
  static constexpr uint32_t DHCSR_DBGKEY = 0xA05F0000UL;
  static constexpr uint32_t DHCSR_C_DEBUGEN = 0x00000001UL;
  static constexpr uint32_t DHCSR_C_HALT = 0x00000002UL;

  static constexpr uint32_t BACKUP_MAGIC = 0x45565342UL;   // "EVSB"
  static constexpr uint32_t BACKUP_FOOTER_MAGIC = 0x45565346UL;  // "EVSF"
  static constexpr uint16_t BACKUP_HEADER_VERSION = 1;
  static constexpr uint8_t BACKUP_STATE_IN_PROGRESS = 0xFE;
  static constexpr uint8_t BACKUP_STATE_VALID = 0xFC;
  static constexpr uint8_t BACKUP_STATE_INVALID = 0xF8;
  static constexpr uint32_t BACKUP_IMAGE_OFFSET = 0x1000UL;
  static constexpr uint16_t BACKUP_IO_BLOCK_SIZE = 64;
  static constexpr const char *MANAGED_MARKER = "EMPORIAVUE-SAMD";
  static constexpr uint8_t MANAGED_MARKER_LENGTH = 15;
  static constexpr uint32_t MANAGED_INFO_MAGIC = 0x4556534DUL;  // "EVSM"
  static constexpr uint16_t MANAGED_INFO_FORMAT_VERSION = 1;
  static constexpr uint16_t STOCK_I2C_FRAME_SIZE = 284;

  struct BackupHeader {
    uint32_t magic;
    uint16_t version;
    uint8_t state;
    uint8_t reserved0;
    uint32_t header_size;
    uint32_t image_offset;
    uint32_t flash_size;
    uint32_t page_size;
    uint32_t page_count;
    uint32_t nvm_param;
    uint32_t dsu_did;
    uint8_t sha256[32];
    uint8_t reserved[64];
  } __attribute__((packed));

  struct BackupFooter {
    uint32_t magic;
    uint32_t flash_size;
    uint8_t sha256[32];
  } __attribute__((packed));

  struct ManagedFirmwareInfo {
    uint32_t magic;
    uint16_t format_version;
    uint16_t hardware_id;
    uint32_t firmware_version;
    uint32_t image_size;
    uint8_t image_sha256[32];
    char marker[MANAGED_MARKER_LENGTH];
    uint8_t reserved1;
  } __attribute__((packed));

  enum MemSize : uint8_t {
    MEM_SIZE_BYTE = 0,
    MEM_SIZE_HALFWORD = 1,
    MEM_SIZE_WORD = 2,
  };

  enum class BackupStage : uint8_t {
    IDLE = 0,
    READ_AND_STORE,
    VERIFY_SECOND_READ,
  };

  enum class InstallStage : uint8_t {
    IDLE = 0,
    FLASH_PAGES,
  };

  enum class FirmwareAction : uint8_t {
    UNKNOWN = 0,
    NONE,
    BACKUP_STOCK,
    UPDATE_MANAGED,
    RESTORE_STOCK,
  };

  enum class FlashSource : uint8_t {
    NONE = 0,
    BUNDLED,
    BACKUP,
  };

  enum class FirmwareKind : uint8_t {
    UNKNOWN = 0,
    STOCK,
    MANAGED,
  };

  struct FirmwareInfo {
    FirmwareKind kind{FirmwareKind::UNKNOWN};
    uint16_t hardware_id{0};
    uint32_t version{0};
    uint32_t flash_size{0};
    uint32_t image_size{0};
    uint32_t page_size{0};
    uint32_t page_count{0};
    uint32_t nvm_param{0};
    uint8_t image_sha256[32]{};
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
  void publish_firmware_status_(const std::string &status);
  void publish_firmware_action_(const std::string &action);
  void publish_firmware_version_(const FirmwareInfo &info);
  void publish_firmware_update_available_(bool available);
  void publish_firmware_restore_available_(bool available);
  void publish_read_allowed_(bool value);
  static std::string hex32_(uint32_t value);
  static std::string hex16_(uint16_t value);
  static std::string hex8_(uint8_t value);
  static void append_hex_byte_(std::string *output, uint8_t value);
  static std::string sha256_hex_(const uint8_t hash[32]);
  static std::string format_firmware_version_(uint32_t version);

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
  bool mem_write8_(uint32_t address, uint8_t value);
  bool mem_write16_(uint32_t address, uint16_t value);
  bool mem_write32_(uint32_t address, uint32_t value);
  bool halt_core_();
  bool resume_core_();
  bool read_flash_geometry_(uint32_t *param, uint32_t *page_size, uint32_t *page_count, uint32_t *flash_size);
  bool dump_flash_block_(uint32_t address, uint16_t length, std::string *hex_data);
  bool read_flash_bytes_(uint32_t address, uint16_t length, uint8_t *data);
  bool power_up_debug_();
  bool verify_mem_ap_();
  void perform_boot_reset_();
  void inspect_backup_partition_();
  bool find_backup_partition_();
  bool backup_partition_has_capacity_(uint32_t flash_size);
  bool read_backup_header_(BackupHeader *header);
  bool hash_partition_image_(uint32_t flash_size, uint8_t hash[32]);
  bool write_backup_header_(uint8_t state, uint32_t flash_size, uint32_t page_size, uint32_t page_count,
                            uint32_t nvm_param, uint32_t dsu_did);
  bool write_backup_state_(uint8_t state);
  bool write_backup_hash_and_footer_(const uint8_t hash[32], uint32_t flash_size);
  bool detect_managed_firmware_(uint32_t flash_size, bool *managed);
  bool read_managed_firmware_info_(uint32_t flash_size, ManagedFirmwareInfo *managed_info, bool *found);
  bool read_current_firmware_info_(FirmwareInfo *info);
  bool read_valid_backup_(BackupHeader *header, std::string *error);
  bool backup_partition_valid_(std::string *error);
  FirmwareAction determine_firmware_action_(const FirmwareInfo &current, const BackupHeader *backup_header,
                                            std::string *reason) const;
  void publish_detected_firmware_action_(FirmwareAction action, const std::string &reason);
  void start_firmware_action_(FirmwareAction requested_action);
  bool bundled_firmware_available_() const;
  uint32_t bundled_firmware_hardware_id_() const;
  uint32_t bundled_firmware_version_() const;
  uint32_t bundled_firmware_size_() const;
  bool nvm_wait_ready_();
  bool nvm_clear_errors_();
  bool nvm_check_errors_();
  bool nvm_command_(uint8_t command);
  bool erase_flash_row_(uint32_t address);
  bool read_install_source_(uint32_t offset, uint32_t length, uint8_t *buffer);
  bool write_flash_page_(uint32_t address, uint32_t offset, uint32_t length);
  bool verify_flash_page_(uint32_t address, uint32_t offset, uint32_t length);
  const char *install_action_name_() const;
  void process_install_();
  void fail_install_(const std::string &error);
  void finish_install_success_();
  void process_backup_();
  void fail_backup_(const std::string &error);
  void finish_backup_success_();

  InternalGPIOPin *swdio_pin_{nullptr};
  InternalGPIOPin *swclk_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
  text_sensor::TextSensor *swd_idcode_sensor_{nullptr};
  text_sensor::TextSensor *dsu_did_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  text_sensor::TextSensor *firmware_status_sensor_{nullptr};
  text_sensor::TextSensor *firmware_action_sensor_{nullptr};
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
  binary_sensor::BinarySensor *read_allowed_sensor_{nullptr};
  binary_sensor::BinarySensor *firmware_update_available_sensor_{nullptr};
  binary_sensor::BinarySensor *firmware_restore_available_sensor_{nullptr};

  uint16_t hardware_id_{0};
  bool reset_before_read_{false};
  bool reset_on_boot_{false};
  bool connect_under_reset_{false};
  uint32_t reset_hold_time_ms_{100};
  uint32_t reset_release_time_ms_{50};
  uint8_t clock_delay_us_{2};
  uint8_t retry_count_{40};
  uint32_t dump_start_address_{FLASH_START};
  uint16_t dump_block_size_{64};
  uint32_t dump_block_count_{5};
  uint32_t dump_effective_block_count_{5};
  uint32_t dump_total_size_{320};
  bool dump_full_flash_{false};
  bool dump_halt_core_{true};
  bool dump_resume_between_blocks_{false};
  bool dump_active_{false};
  bool dump_core_halted_{false};
  uint32_t dump_next_block_{0};
  std::string backup_partition_name_{"samd_bak"};
  uint32_t required_firmware_version_{10};
  bool allow_samd_write_{false};
  bool require_backup_before_install_{true};
  const esp_partition_t *backup_partition_{nullptr};
  bool backup_active_{false};
  bool backup_core_halted_{false};
  bool backup_header_written_{false};
  BackupStage backup_stage_{BackupStage::IDLE};
  uint32_t backup_next_offset_{0};
  uint32_t backup_flash_size_{0};
  uint32_t backup_page_size_{0};
  uint32_t backup_page_count_{0};
  uint32_t backup_nvm_param_{0};
  uint32_t backup_dsu_did_{0};
  std::array<uint8_t, 32> backup_stored_hash_{};
  std::array<uint8_t, 32> backup_verify_hash_{};
  mbedtls_sha256_context backup_sha_ctx_{};
  bool backup_sha_ctx_active_{false};
  bool install_active_{false};
  bool install_core_halted_{false};
  bool install_started_writing_{false};
  FirmwareAction install_action_{FirmwareAction::UNKNOWN};
  FlashSource install_source_{FlashSource::NONE};
  InstallStage install_stage_{InstallStage::IDLE};
  BackupHeader install_backup_header_{};
  uint32_t install_next_offset_{0};
  uint32_t install_flash_size_{0};
  uint32_t install_page_size_{0};
  uint32_t install_row_size_{0};
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

class EmporiaVueBackupFirmwareButton : public button::Button, public Parented<EmporiaVueComponent> {
 protected:
  void press_action() override { this->parent_->backup_firmware(); }
};

class EmporiaVueInstallFirmwareButton : public button::Button, public Parented<EmporiaVueComponent> {
 protected:
  void press_action() override { this->parent_->install_firmware(); }
};

class EmporiaVueRestoreFirmwareButton : public button::Button, public Parented<EmporiaVueComponent> {
 protected:
  void press_action() override { this->parent_->restore_firmware(); }
};

}  // namespace emporiavue
}  // namespace esphome
