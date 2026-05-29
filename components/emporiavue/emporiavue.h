#pragma once

#include "esphome/components/button/button.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
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

class EmporiaVueComponent : public Component, public i2c::I2CDevice {
 public:
  enum class RuntimeMode : uint8_t {
    I2C = 0,
    SPI = 1,
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA + 10.0f; }

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
  void set_runtime_mode(uint8_t mode) {
    this->runtime_mode_ = mode == 1 ? RuntimeMode::SPI : RuntimeMode::I2C;
  }
  void set_entity_prefix(const std::string &entity_prefix) { this->entity_prefix_ = entity_prefix; }
  void set_auto_update_samd(bool auto_update_samd) { this->auto_update_samd_ = auto_update_samd; }
  void set_diagnostics_interval(uint32_t diagnostics_interval_ms) {
    this->diagnostics_interval_ms_ = diagnostics_interval_ms;
  }
  void set_backup_partition_name(const std::string &backup_partition_name) {
    this->backup_partition_name_ = backup_partition_name;
  }
  void set_firmware_version_sensor(text_sensor::TextSensor *sensor) { this->firmware_version_sensor_ = sensor; }
  void set_bundled_firmware_version_sensor(text_sensor::TextSensor *sensor) {
    this->bundled_firmware_version_sensor_ = sensor;
  }
  void set_diag_sample_blocks_sensor(sensor::Sensor *sensor) { this->diag_sample_blocks_sensor_ = sensor; }
  void set_diag_packets_built_sensor(sensor::Sensor *sensor) { this->diag_packets_built_sensor_ = sensor; }
  void set_diag_packets_read_sensor(sensor::Sensor *sensor) { this->diag_packets_read_sensor_ = sensor; }
  void set_diag_dma_transfer_errors_sensor(sensor::Sensor *sensor) {
    this->diag_dma_transfer_errors_sensor_ = sensor;
  }
  void set_diag_packet_overruns_sensor(sensor::Sensor *sensor) { this->diag_packet_overruns_sensor_ = sensor; }
  void set_diag_i2c_partial_reads_sensor(sensor::Sensor *sensor) {
    this->diag_i2c_partial_reads_sensor_ = sensor;
  }
  void set_diag_last_sample_count_sensor(sensor::Sensor *sensor) { this->diag_last_sample_count_sensor_ = sensor; }

  void backup_firmware();
  void install_firmware();
  void restore_firmware();
  void flash_external_firmware(uint8_t index = 0);

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
  static constexpr uint32_t INSTALL_PROGRESS_LOG_INTERVAL = 3072;
  static constexpr uint32_t FLASH_START = 0x00000000UL;
  static constexpr uint32_t DHCSR = 0xE000EDF0UL;
  static constexpr uint32_t DCRSR = 0xE000EDF4UL;
  static constexpr uint32_t DCRDR = 0xE000EDF8UL;
  static constexpr uint32_t DHCSR_DBGKEY = 0xA05F0000UL;
  static constexpr uint32_t DHCSR_C_DEBUGEN = 0x00000001UL;
  static constexpr uint32_t DHCSR_C_HALT = 0x00000002UL;
  static constexpr uint32_t DHCSR_S_REGRDY = 0x00010000UL;
  static constexpr uint32_t DHCSR_S_HALT = 0x00020000UL;
  static constexpr uint8_t CORE_REG_SP = 13;
  static constexpr uint8_t CORE_REG_LR = 14;
  static constexpr uint8_t CORE_REG_PC = 15;
  static constexpr uint8_t CORE_REG_XPSR = 16;
  static constexpr uint32_t ICSR = 0xE000ED04UL;
  static constexpr uint32_t VTOR = 0xE000ED08UL;
  static constexpr uint32_t AIRCR = 0xE000ED0CUL;
  static constexpr uint32_t AIRCR_VECTKEY = 0x05FA0000UL;
  static constexpr uint32_t AIRCR_SYSRESETREQ = 0x00000004UL;
  static constexpr uint32_t GCLK_STATUS = 0x40000C01UL;
  static constexpr uint32_t ADC_STATUS = 0x42002019UL;
  static constexpr uint32_t TC1_STATUS = 0x4200180FUL;
  static constexpr uint32_t SERCOM1_I2CS_CTRLA = 0x42000C00UL;
  static constexpr uint32_t SERCOM1_I2CS_CTRLB = 0x42000C04UL;
  static constexpr uint32_t SERCOM1_I2CS_INTENSET = 0x42000C16UL;
  static constexpr uint32_t SERCOM1_I2CS_INTFLAG = 0x42000C18UL;
  static constexpr uint32_t SERCOM1_I2CS_STATUS = 0x42000C1AUL;
  static constexpr uint32_t SERCOM1_I2CS_SYNCBUSY = 0x42000C1CUL;
  static constexpr uint32_t SERCOM1_I2CS_ADDR = 0x42000C24UL;
  static constexpr uint32_t SAMD_BOOT_DIAG_BASE = 0x20000000UL;
  static constexpr uint32_t SAMD_BOOT_DIAG_MAGIC = 0x45565344UL;  // "EVSD"

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
  static constexpr uint32_t LEGACY_MANAGED_INFO_MAGIC = 0x4556534DUL;  // "EVSM"
  static constexpr uint16_t MANAGED_INFO_FORMAT_VERSION = 1;
  static constexpr uint16_t MANAGED_MODE_I2C = 1;
  static constexpr uint16_t MANAGED_MODE_SPI = 2;
  static constexpr uint16_t STOCK_I2C_FRAME_SIZE = 284;
  static constexpr uint8_t MANAGED_I2C_DIAGNOSTIC_COMMAND = 0xF1;

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
    uint16_t hardware_id;
    uint16_t mode_id;
    uint32_t firmware_version;
    uint8_t image_sha256[32];
    char marker[MANAGED_MARKER_LENGTH];
    uint8_t reserved[9];
  } __attribute__((packed));

  struct LegacyMagicManagedFirmwareInfo {
    uint32_t magic;
    uint32_t firmware_version;
    uint16_t hardware_id;
    uint8_t image_sha256[32];
    char marker[MANAGED_MARKER_LENGTH];
    uint8_t reserved[7];
  } __attribute__((packed));

  struct LegacyManagedFirmwareInfo {
    uint32_t magic;
    uint16_t format_version;
    uint16_t hardware_id;
    uint32_t firmware_version;
    uint32_t image_size;
    uint8_t image_sha256[32];
    char marker[MANAGED_MARKER_LENGTH];
    uint8_t reserved1;
  } __attribute__((packed));

  struct ManagedI2CDiagnostic {
    uint16_t hardware_id;
    uint32_t firmware_version;
    uint16_t i2c_frame_length;
    uint32_t diagnostic_sequence;
    uint32_t sample_blocks;
    uint32_t packets_built;
    uint32_t packets_read;
    uint32_t dma_transfer_errors;
    uint32_t packet_overruns;
    uint32_t i2c_partial_reads;
    uint32_t i2c_oversize_reads;
    uint16_t last_sample_count;
    uint16_t last_i2c_read_len;
    uint32_t crc32;
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
    UPDATE_MANAGED,
    RESTORE_STOCK,
    FLASH_EXTERNAL,
  };

  enum class FlashSource : uint8_t {
    NONE = 0,
    BUNDLED,
    BACKUP,
    EXTERNAL,
  };

  enum class FirmwareKind : uint8_t {
    UNKNOWN = 0,
    STOCK,
    MANAGED,
  };

  enum class FirmwareDetectionSource : uint8_t {
    UNKNOWN = 0,
    SWD,
    I2C,
  };

  enum class ManagedI2CDiagnosticResult : uint8_t {
    I2C_ERROR = 0,
    INVALID_RESPONSE,
    VALID_RESPONSE,
  };

  struct FirmwareInfo {
    FirmwareKind kind{FirmwareKind::UNKNOWN};
    uint16_t hardware_id{0};
    uint16_t mode_id{0};
    uint32_t version{0};
    uint32_t flash_size{0};
    uint32_t image_size{0};
    uint16_t i2c_frame_length{0};
    FirmwareDetectionSource source{FirmwareDetectionSource::UNKNOWN};
    uint32_t page_size{0};
    uint32_t page_count{0};
    uint32_t nvm_param{0};
    uint8_t image_sha256[32]{};
  };

  void reset_target_();
  void assert_reset_();
  void deassert_reset_();
  void deassert_reset_for_swd_attach_();
  bool connect_under_reset_active_() const;
  void cold_plug_swd_();
  void begin_swd_session_();
  void finish_swd_session_();
  void swd_enter_debug_(uint8_t swj_select_bits);
  bool swd_initialize_(uint32_t *idcode);
  void prepare_pins_();
  void release_pins_();
  void set_error_(const std::string &error);
  void publish_status_(const std::string &status);
  void publish_firmware_status_(const std::string &status);
  void publish_firmware_version_(const FirmwareInfo &info);
  void publish_bundled_firmware_version_();
  static std::string hex32_(uint32_t value);
  static std::string hex16_(uint16_t value);
  static std::string hex8_(uint8_t value);
  static void append_hex_byte_(std::string *output, uint8_t value);
  static std::string sha256_hex_(const uint8_t hash[32]);
  static std::string format_firmware_version_(uint32_t version);
  static const char *firmware_mode_name_(uint16_t mode_id);
  static uint32_t crc32_(const uint8_t *data, size_t length);

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
  bool system_reset_core_();
  bool read_core_register_(uint8_t reg, uint32_t *value);
  bool read_flash_geometry_(uint32_t *param, uint32_t *page_size, uint32_t *page_count, uint32_t *flash_size);
  bool read_flash_bytes_(uint32_t address, uint16_t length, uint8_t *data);
  bool flash_row_erased_(uint32_t address, uint32_t row_size, bool *erased);
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
  ManagedI2CDiagnosticResult query_managed_i2c_diagnostic_(ManagedI2CDiagnostic *diagnostic);
  bool validate_managed_i2c_diagnostic_(const ManagedI2CDiagnostic &diagnostic) const;
  void refresh_i2c_diagnostics_();
  void publish_firmware_info_from_diagnostic_(const ManagedI2CDiagnostic &diagnostic);
  void publish_i2c_diagnostics_(const ManagedI2CDiagnostic &diagnostic);
  void start_i2c_diagnostics_();
  bool firmware_mode_matches_runtime_() const;
  void publish_firmware_mode_mismatch_();
  void probe_runtime_i2c_after_firmware_update_();
  void publish_initial_firmware_detection_();
  bool detect_current_firmware_by_swd_(FirmwareInfo *info, std::string *error);
  bool should_auto_update_samd_(const FirmwareInfo &info, std::string *reason) const;
  bool detect_managed_firmware_(uint32_t flash_size, bool *managed);
  bool read_managed_firmware_info_(uint32_t flash_size, ManagedFirmwareInfo *managed_info, bool *found);
  bool read_current_firmware_info_(FirmwareInfo *info);
  bool read_valid_backup_(BackupHeader *header, std::string *error);
  FirmwareAction determine_firmware_action_(const FirmwareInfo &current, std::string *reason) const;
  void start_firmware_action_(FirmwareAction requested_action, bool force_update, uint8_t external_firmware_index = 0);
  bool bundled_firmware_available_() const;
  bool bundled_firmware_matches_target_() const;
  uint32_t bundled_firmware_hardware_id_() const;
  uint32_t bundled_firmware_mode_id_() const;
  uint32_t bundled_firmware_version_() const;
  uint32_t bundled_firmware_size_() const;
  bool external_firmware_available_(uint8_t index) const;
  uint32_t external_firmware_size_(uint8_t index) const;
  const uint8_t *external_firmware_data_(uint8_t index) const;
  uint16_t expected_firmware_mode_id_() const;
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
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
  text_sensor::TextSensor *bundled_firmware_version_sensor_{nullptr};
  sensor::Sensor *diag_sample_blocks_sensor_{nullptr};
  sensor::Sensor *diag_packets_built_sensor_{nullptr};
  sensor::Sensor *diag_packets_read_sensor_{nullptr};
  sensor::Sensor *diag_dma_transfer_errors_sensor_{nullptr};
  sensor::Sensor *diag_packet_overruns_sensor_{nullptr};
  sensor::Sensor *diag_i2c_partial_reads_sensor_{nullptr};
  sensor::Sensor *diag_last_sample_count_sensor_{nullptr};

  uint16_t hardware_id_{0};
  bool reset_before_read_{false};
  bool reset_on_boot_{false};
  bool connect_under_reset_{false};
  bool target_reset_asserted_{false};
  uint32_t reset_hold_time_ms_{100};
  uint32_t reset_release_time_ms_{50};
  uint8_t clock_delay_us_{2};
  uint8_t retry_count_{40};
  RuntimeMode runtime_mode_{RuntimeMode::I2C};
  std::string entity_prefix_{};
  bool auto_update_samd_{false};
  uint32_t diagnostics_interval_ms_{0};
  bool diagnostics_started_{false};
  std::string backup_partition_name_{"samd_bak"};
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
  FirmwareInfo detected_firmware_info_{};
  bool detected_firmware_info_valid_{false};
  FirmwareAction install_action_{FirmwareAction::UNKNOWN};
  FlashSource install_source_{FlashSource::NONE};
  InstallStage install_stage_{InstallStage::IDLE};
  BackupHeader install_backup_header_{};
  uint32_t install_next_offset_{0};
  uint32_t install_next_progress_log_offset_{0};
  uint32_t install_flash_size_{0};
  uint32_t install_page_size_{0};
  uint32_t install_row_size_{0};
  uint8_t install_external_firmware_index_{0};
  bool init_pins_on_boot_{false};
  bool pins_setup_{false};
  bool direction_write_{true};
  bool sample_before_clock_{false};
  bool selected_ap_valid_{false};
  uint8_t selected_ap_bank_{0};
  uint32_t cached_csw_{0xFFFFFFFFUL};
  std::string last_error_;
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

class EmporiaVueFlashExternalFirmwareButton : public button::Button, public Parented<EmporiaVueComponent> {
 public:
  void set_firmware_index(uint8_t index) { this->firmware_index_ = index; }

 protected:
  void press_action() override { this->parent_->flash_external_firmware(this->firmware_index_); }

  uint8_t firmware_index_{0};
};

}  // namespace emporiavue
}  // namespace esphome
