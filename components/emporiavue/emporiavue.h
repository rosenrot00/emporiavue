#pragma once

#include "esphome/components/button/button.h"
#ifdef USE_I2C
#include "esphome/components/i2c/i2c.h"
#endif
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include <esp_partition.h>
#include <mbedtls/sha256.h>
#ifdef USE_ESP32
#include <driver/spi_slave.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace emporiavue {

static constexpr const char *TAG = "emporiavue";
static constexpr float VUE2_STOCK_CYCLE_TIMEBASE_HZ = 25310.0f;
static constexpr float VUE3_STOCK_CYCLE_TIMEBASE_HZ = 19610.0f;
static constexpr uint8_t SPI_RX_QUEUE_SIZE = 16;

class MeteringPhaseConfig;
class MeteringCTClampConfig;
class MeteringGroupConfig;
class MeteringVirtualLineConfig;
class MeteringCalibrationNumber;

class MeteringPowerFilters {
 public:
  void add_multiply_filter(float multiplier) {
    this->filters_.push_back([multiplier](float value) -> float { return value * multiplier; });
  }
  void add_lambda_filter(std::function<float(float)> filter) { this->filters_.push_back(std::move(filter)); }
  float apply(float value) const {
    for (const auto &filter : this->filters_) {
      value = filter(value);
    }
    return value;
  }
  size_t size() const { return this->filters_.size(); }

 protected:
  std::vector<std::function<float(float)>> filters_{};
};

class MeteringPowerOutput {
 public:
  enum Direction : uint8_t {
    DIRECTION_SIGNED = 0,
    DIRECTION_POSITIVE = 1,
    DIRECTION_NEGATIVE = 2,
  };

  MeteringPowerOutput(uint8_t direction, sensor::Sensor *raw_power_sensor, sensor::Sensor *power_sensor)
      : direction_(direction), raw_power_sensor_(raw_power_sensor), power_sensor_(power_sensor) {}

  uint8_t get_direction() const { return this->direction_; }
  sensor::Sensor *get_raw_power_sensor() const { return this->raw_power_sensor_; }
  sensor::Sensor *get_power_sensor() const { return this->power_sensor_; }

 protected:
  uint8_t direction_{DIRECTION_SIGNED};
  sensor::Sensor *raw_power_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
};

class EmporiaVueComponent : public Component
#ifdef USE_I2C
                             ,
                             public i2c::I2CDevice
#endif
{
 public:
  enum class RuntimeMode : uint8_t {
    I2C = 0,
    SPI = 1,
  };

  enum class MeteringTransport : uint8_t {
    UNKNOWN = 0,
    I2C = 1,
    SPI = 2,
  };

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA + 10.0f; }

  void set_swdio_pin(InternalGPIOPin *pin) { this->swdio_pin_ = pin; }
  void set_swclk_pin(InternalGPIOPin *pin) { this->swclk_pin_ = pin; }
  void set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }
  void set_spi_clk_pin(uint8_t pin) { this->spi_clk_pin_ = pin; }
  void set_spi_data_pin(uint8_t pin) { this->spi_data_pin_ = pin; }
  void set_spi_frame_pin(uint8_t pin) { this->spi_frame_pin_ = pin; }
  void set_spi_main_current_delay(uint8_t delay) { this->spi_main_current_delay_ = delay; }
  void set_spi_mux_current_delay(uint8_t delay) { this->spi_mux_current_delay_ = delay; }
  void set_hardware_id(uint16_t hardware_id) { this->hardware_id_ = hardware_id; }
  void set_connect_under_reset(bool connect_under_reset) { this->connect_under_reset_ = connect_under_reset; }
  void set_reset_release_time(uint32_t reset_release_time) { this->reset_release_time_ms_ = reset_release_time; }
  void set_clock_delay_us(uint8_t clock_delay_us) { this->clock_delay_us_ = clock_delay_us; }
  void set_runtime_mode(uint8_t mode) {
    this->runtime_mode_ = mode == 1 ? RuntimeMode::SPI : RuntimeMode::I2C;
  }
  void set_entity_prefix(const std::string &entity_prefix) { this->entity_prefix_ = entity_prefix; }
  void set_auto_update_samd(bool auto_update_samd) { this->auto_update_samd_ = auto_update_samd; }
  void set_diagnostics_interval(uint32_t diagnostics_interval_ms) {
    this->diagnostics_interval_ms_ = diagnostics_interval_ms;
  }
  void set_metering_interval(uint32_t metering_interval_ms) { this->metering_interval_ms_ = metering_interval_ms; }
  void set_power_apparent_min(float power_apparent_min) { this->power_apparent_min_ = power_apparent_min; }
  void set_phase_detection_confidence_ratio(float confidence_ratio) {
    this->phase_detection_confidence_ratio_ = confidence_ratio;
  }
  void set_phase_detection_update_interval(uint32_t update_interval_ms) {
    this->phase_detection_update_interval_ms_ = update_interval_ms;
  }
  void set_metering_phases(std::vector<MeteringPhaseConfig *> phases) {
    this->metering_phases_ = std::move(phases);
  }
  void set_metering_ct_clamps(std::vector<MeteringCTClampConfig *> ct_clamps) {
    this->metering_ct_clamps_ = std::move(ct_clamps);
  }
  void set_metering_groups(std::vector<MeteringGroupConfig *> groups) {
    this->metering_groups_ = std::move(groups);
  }
  void set_metering_virtual_lines(std::vector<MeteringVirtualLineConfig *> virtual_lines) {
    this->metering_virtual_lines_ = std::move(virtual_lines);
  }
  void set_backup_partition_name(const std::string &backup_partition_name) {
    this->backup_partition_name_ = backup_partition_name;
  }
  void set_firmware_version_sensor(text_sensor::TextSensor *sensor) { this->firmware_version_sensor_ = sensor; }
  void set_bundled_firmware_version_sensor(text_sensor::TextSensor *sensor) {
    this->bundled_firmware_version_sensor_ = sensor;
  }
  void set_diag_frame_errors_sensor(sensor::Sensor *sensor) { this->diag_frame_errors_sensor_ = sensor; }
  void set_diag_transfer_errors_sensor(sensor::Sensor *sensor) { this->diag_transfer_errors_sensor_ = sensor; }
  void set_diag_frame_overruns_sensor(sensor::Sensor *sensor) { this->diag_frame_overruns_sensor_ = sensor; }
  void set_diag_recoveries_sensor(sensor::Sensor *sensor) { this->diag_recoveries_sensor_ = sensor; }
  void set_diag_last_frame_samples_sensor(sensor::Sensor *sensor) { this->diag_last_frame_samples_sensor_ = sensor; }
  void set_diag_sample_rate_sensor(sensor::Sensor *sensor) { this->diag_sample_rate_sensor_ = sensor; }

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
  static constexpr uint8_t SWD_RETRY_COUNT = 40;
  static constexpr uint16_t SWD_ATTACH_RESET_HOLD_US = 100;
  static constexpr uint16_t RESET_PULSE_MS = 1;

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

  enum class I2CMeteringReadResult : uint8_t {
    VALID_FRAME,
    STALE_FRAME,
    ERROR,
  };

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

  struct I2CPowerEntry {
    int32_t phase[3];
  } __attribute__((packed));

  struct I2CMeteringPacket {
    uint8_t is_unread;
    uint8_t checksum;
    uint8_t unknown;
    uint8_t sequence_num;
    I2CPowerEntry power[19];
    uint16_t voltage[3];
    uint16_t cycle_count[3];
    uint16_t current[19];
    uint16_t end;
  } __attribute__((packed));

  struct MeteringPhase {
    uint16_t voltage_raw{0};
    uint16_t cycle_count_raw{0};
    float frequency_hz{std::numeric_limits<float>::quiet_NaN()};
    float phase_angle_degrees{std::numeric_limits<float>::quiet_NaN()};
    uint8_t quality_flags{0};
  };

  struct MeteringClamp {
    uint16_t current_raw{0};
    int32_t power_raw_by_phase[3]{};
    uint8_t quality_flags{0};
  };

  struct MeteringFrame {
    uint16_t schema_version{1};
    MeteringTransport transport{MeteringTransport::UNKNOWN};
    uint32_t sequence{0};
    uint32_t timestamp_ms{0};
    bool valid{false};
    uint8_t quality_flags{0};
    MeteringPhase phases[3]{};
    MeteringClamp clamps[19]{};
  };

  struct MeteringCTMeasurement {
    bool has_power{false};
    float power{0.0f};
    bool has_voltage{false};
    float voltage{0.0f};
    bool has_current{false};
    float current{0.0f};
    bool has_apparent_power{false};
    float apparent_power{0.0f};
    bool has_power_factor{false};
    float power_factor{0.0f};
  };

  struct SpiRawScan {
    int16_t value[8]{};
    uint32_t sample_counter{0};
    uint8_t mux_index{0};
  };

  struct SpiMeteringAccumulator {
    int32_t current_sum[19]{};
    int64_t voltage_square_sum[3]{};
    int32_t voltage_sum[3]{};
    int64_t current_square_sum[19]{};
    int64_t raw_power_sum[19][3]{};
    double voltage_fund_i[3]{};
    double voltage_fund_q[3]{};
    float cycle_sum[3]{};
    float line1_periods[64]{};
    float dft_period_samples{0.0f};
    uint16_t cycle_count[3]{};
    uint16_t line1_period_count{0};
    uint16_t target_periods{0};
    uint8_t line1_period_sample_count{0};
    uint32_t window_start_sample{0};
    uint32_t sample_count{0};
    uint32_t mux_sample_count[16]{};
  };

  enum MemSize : uint8_t {
    MEM_SIZE_BYTE = 0,
    MEM_SIZE_HALFWORD = 1,
    MEM_SIZE_WORD = 2,
  };

  enum class BackupStage : uint8_t {
    IDLE = 0,
    READ_AND_STORE,
    READ_AND_LOG,
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

  void assert_reset_();
  void assert_reset_for_swd_attach_();
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
  void publish_spi_diagnostics_();
  void start_i2c_diagnostics_();
  void stop_i2c_diagnostics_();
  I2CMeteringReadResult read_i2c_metering_frame_(MeteringFrame *frame);
  uint8_t calculate_i2c_metering_checksum_(const I2CMeteringPacket &packet) const;
  bool decode_i2c_metering_packet_(const I2CMeteringPacket &packet, MeteringFrame *frame) const;
  void submit_metering_frame_(const MeteringFrame &frame);
  void publish_metering_frame_(const MeteringFrame &frame);
  void refresh_metering_();
  void start_metering_();
  void stop_metering_();
  void setup_spi_receiver_(bool reset_statistics = true);
  bool stop_spi_receiver_(bool release_driver = true);
  void restart_spi_receiver_();
  void process_spi_receiver_();
#ifdef USE_ESP32
  void process_spi_transaction_(spi_slave_transaction_t *transaction);
  void spi_rx_task_();
  static void spi_rx_task_trampoline_(void *arg);
#endif
  bool queue_spi_receive_(uint8_t index);
  bool validate_spi_frame_(const uint8_t *frame, uint32_t *sequence, uint32_t *flags, uint32_t *sample_counter,
                           uint16_t *sample_period_ticks) const;
  void reset_spi_metering_state_();
  void decode_spi_raw_frame_(const uint8_t *frame, uint32_t sequence, uint32_t flags, uint32_t sample_counter);
  void process_spi_raw_scan_(const SpiRawScan &scan);
  void finish_spi_metering_window_(uint32_t sequence, uint32_t flags);
  uint32_t spi_metering_target_samples_() const;
  uint16_t spi_metering_target_periods_(float period_samples) const;
  static int16_t sanitize_spi_adc_offset_(int32_t average);
  static uint16_t scale_spi_rms_(uint64_t sum, uint32_t numerator, uint32_t denominator);
  static int32_t scale_spi_power_(int64_t raw, uint32_t multiplier, uint32_t denominator);
  void publish_queued_spi_metering_();
  void start_firmware_mode_mismatch_log_();
  void stop_firmware_mode_mismatch_log_();
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
  const uint8_t *bundled_firmware_data_() const;
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
  bool calculate_ct_power_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp, float *power) const;
  bool calculate_phase_voltage_(const MeteringFrame &frame, const MeteringPhaseConfig *phase, float *voltage) const;
  bool calculate_line_to_line_voltage_(const MeteringFrame &frame, const MeteringPhaseConfig *line_a,
                                       const MeteringPhaseConfig *line_b, float *voltage) const;
  bool calculate_ct_voltage_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp, float *voltage) const;
  bool calculate_ct_current_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp, float *current) const;
  bool calculate_ct_apparent_power_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp,
                                    float *apparent_power) const;
  bool calculate_ct_measurement_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp,
                                 MeteringCTMeasurement *measurement) const;
  bool calculate_group_power_(const MeteringFrame &frame, const MeteringGroupConfig *group, float *group_power,
                              uint8_t depth = 0) const;
  static float apply_power_direction_(float power, uint8_t direction);
  static void publish_power_outputs_(const std::vector<MeteringPowerOutput> &outputs, float power);
  void update_phase_detection_(const MeteringFrame &frame, MeteringCTClampConfig *ct_clamp);

  InternalGPIOPin *swdio_pin_{nullptr};
  InternalGPIOPin *swclk_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
  uint8_t spi_clk_pin_{22};
  uint8_t spi_data_pin_{21};
  uint8_t spi_frame_pin_{13};
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
  text_sensor::TextSensor *bundled_firmware_version_sensor_{nullptr};
  sensor::Sensor *diag_frame_errors_sensor_{nullptr};
  sensor::Sensor *diag_transfer_errors_sensor_{nullptr};
  sensor::Sensor *diag_frame_overruns_sensor_{nullptr};
  sensor::Sensor *diag_recoveries_sensor_{nullptr};
  sensor::Sensor *diag_last_frame_samples_sensor_{nullptr};
  sensor::Sensor *diag_sample_rate_sensor_{nullptr};

  uint16_t hardware_id_{0};
  bool connect_under_reset_{false};
  bool target_reset_asserted_{false};
  uint32_t reset_release_time_ms_{50};
  uint8_t clock_delay_us_{2};
  RuntimeMode runtime_mode_{RuntimeMode::I2C};
  std::string entity_prefix_{};
  bool auto_update_samd_{false};
  uint32_t diagnostics_interval_ms_{0};
  bool diagnostics_started_{false};
  bool firmware_mode_mismatch_log_started_{false};
  uint32_t metering_interval_ms_{0};
  bool metering_started_{false};
  uint32_t last_metering_sequence_{0};
  MeteringTransport last_metering_transport_{MeteringTransport::UNKNOWN};
  bool last_metering_sequence_valid_{false};
  MeteringFrame last_metering_frame_{};
  bool spi_receiver_started_{false};
#ifdef USE_ESP32
  spi_host_device_t spi_host_{SPI2_HOST};
  spi_slave_transaction_t spi_transactions_[SPI_RX_QUEUE_SIZE]{};
  uint8_t *spi_rx_buffers_[SPI_RX_QUEUE_SIZE]{};
  QueueHandle_t spi_metering_queue_{nullptr};
  TaskHandle_t spi_rx_task_handle_{nullptr};
  volatile bool spi_rx_task_stop_{false};
#endif
  SpiMeteringAccumulator spi_metering_accumulator_{};
  bool spi_metering_window_synced_{false};
  SpiRawScan spi_raw_scan_ring_[6]{};
  uint8_t spi_raw_scan_ring_index_{0};
  uint8_t spi_raw_scan_ring_count_{0};
  bool spi_expected_sample_counter_valid_{false};
  uint32_t spi_expected_sample_counter_{0};
  uint8_t spi_main_current_delay_{2};
  uint8_t spi_mux_current_delay_{4};
  int16_t spi_adc_offsets_[22]{};
  int32_t spi_adc_offset_estimate_q8_[22]{};
  uint8_t spi_offset_warmup_windows_{0};
  bool spi_last_cross_sample_valid_[3]{};
  float spi_last_cross_sample_[3]{};
  bool spi_line1_period_valid_{false};
  float spi_line1_period_samples_{0.0f};
  bool spi_last_voltage_sample_valid_[3]{};
  int32_t spi_last_voltage_difference_[3]{};
  uint32_t spi_last_voltage_sample_counter_[3]{};
  bool spi_pending_cross_sample_valid_[3]{};
  float spi_pending_cross_sample_[3]{};
  uint8_t spi_cycle_state_[3]{};
  volatile uint16_t spi_rx_inflight_{0};
  uint32_t spi_rx_frames_{0};
  uint32_t spi_rx_sync_errors_{0};
  uint32_t spi_rx_crc_errors_{0};
  uint32_t spi_rx_queue_errors_{0};
  uint32_t spi_rx_frame_gaps_{0};
  uint32_t spi_rx_recoveries_{0};
  uint32_t spi_rx_last_sequence_{0};
  uint32_t spi_rx_last_flags_{0};
  uint32_t spi_rx_last_sample_counter_{0};
  uint32_t spi_last_frame_samples_{0};
  float spi_sample_rate_hz_{0.0f};
  uint32_t spi_invalid_window_last_log_ms_{0};
  uint32_t spi_rx_last_log_ms_{0};
  bool spi_rx_logged_status_valid_{false};
  uint32_t spi_rx_logged_sync_errors_{0};
  uint32_t spi_rx_logged_crc_errors_{0};
  uint32_t spi_rx_logged_queue_errors_{0};
  uint32_t spi_rx_logged_frame_gaps_{0};
  uint32_t spi_rx_logged_recoveries_{0};
  uint32_t spi_rx_logged_flags_{0};
  uint32_t spi_last_diagnostics_publish_ms_{0};
  bool spi_diag_recoveries_published_{false};
  uint32_t spi_diag_published_recoveries_{0};
  volatile uint16_t spi_rx_invalid_streak_{0};
  volatile bool spi_rx_recover_requested_{false};
  bool spi_rx_waiting_for_valid_after_recovery_{false};
  uint8_t spi_rx_recoveries_since_valid_{0};
  uint8_t spi_rx_last_recovery_attempts_{0};
  uint32_t spi_rx_last_samd_reset_ms_{0};
  float power_apparent_min_{5.0f};
  float phase_detection_confidence_ratio_{1.5f};
  uint32_t phase_detection_update_interval_ms_{10000};
  std::vector<MeteringPhaseConfig *> metering_phases_{};
  std::vector<MeteringCTClampConfig *> metering_ct_clamps_{};
  std::vector<MeteringGroupConfig *> metering_groups_{};
  std::vector<MeteringVirtualLineConfig *> metering_virtual_lines_{};
  std::string backup_partition_name_{"samd_bak"};
  const esp_partition_t *backup_partition_{nullptr};
  bool backup_active_{false};
  bool backup_core_halted_{false};
  bool backup_header_written_{false};
  bool backup_log_only_{false};
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
  bool pins_setup_{false};
  bool direction_write_{true};
  bool sample_before_clock_{false};
  bool selected_ap_valid_{false};
  uint8_t selected_ap_bank_{0};
  uint32_t cached_csw_{0xFFFFFFFFUL};
  std::string last_error_;
};

class MeteringPhaseConfig {
 public:
  void set_input_wire(uint8_t input_wire) { this->input_wire_ = input_wire; }
  uint8_t get_input_wire() const { return this->input_wire_; }
  void set_calibration(float calibration) { this->calibration_ = calibration; }
  float get_calibration() const { return this->calibration_; }
  void set_calibration_number(MeteringCalibrationNumber *number) { this->calibration_number_ = number; }
  MeteringCalibrationNumber *get_calibration_number() const { return this->calibration_number_; }
  void setup_calibration_number();
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  sensor::Sensor *get_voltage_sensor() const { return this->voltage_sensor_; }
  void set_frequency_sensor(sensor::Sensor *sensor) { this->frequency_sensor_ = sensor; }
  sensor::Sensor *get_frequency_sensor() const { return this->frequency_sensor_; }
  void set_phase_angle_sensor(sensor::Sensor *sensor) { this->phase_angle_sensor_ = sensor; }
  sensor::Sensor *get_phase_angle_sensor() const { return this->phase_angle_sensor_; }

 protected:
  uint8_t input_wire_{0};
  float calibration_{0.022f};
  MeteringCalibrationNumber *calibration_number_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *phase_angle_sensor_{nullptr};
};

class MeteringCalibrationNumber : public number::Number, public Parented<MeteringPhaseConfig> {
 public:
  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }
  void setup_value();

 protected:
  void control(float value) override;
  void ensure_preference_();

  float initial_value_{0.022f};
  ESPPreferenceObject pref_{};
  bool pref_initialized_{false};
};

class MeteringCTClampConfig {
 public:
  struct PhaseDetectionCandidate {
    MeteringPhaseConfig *phase{nullptr};
    uint8_t line{0};
  };

  void set_phase(MeteringPhaseConfig *phase) {
    this->phase_ = phase;
    this->line_pair_ = false;
    this->line_pair_phase_b_ = nullptr;
  }
  void set_line_pair(MeteringPhaseConfig *phase_a, MeteringPhaseConfig *phase_b) {
    this->phase_ = phase_a;
    this->line_pair_phase_b_ = phase_b;
    this->line_pair_ = true;
  }
  const MeteringPhaseConfig *get_phase() const { return this->phase_; }
  const MeteringPhaseConfig *get_line_pair_phase_b() const { return this->line_pair_phase_b_; }
  bool is_line_pair() const { return this->line_pair_; }
  void set_input_port(uint8_t input_port) { this->input_port_ = input_port; }
  uint8_t get_input_port() const { return this->input_port_; }
  void add_power_output(uint8_t direction, sensor::Sensor *raw_power_sensor, sensor::Sensor *power_sensor) {
    this->power_outputs_.emplace_back(direction, raw_power_sensor, power_sensor);
  }
  const std::vector<MeteringPowerOutput> &get_power_outputs() const { return this->power_outputs_; }
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sensor_ = sensor; }
  sensor::Sensor *get_current_sensor() const { return this->current_sensor_; }
  void set_apparent_power_sensor(sensor::Sensor *sensor) { this->apparent_power_sensor_ = sensor; }
  sensor::Sensor *get_apparent_power_sensor() const { return this->apparent_power_sensor_; }
  void set_power_factor_sensor(sensor::Sensor *sensor) { this->power_factor_sensor_ = sensor; }
  sensor::Sensor *get_power_factor_sensor() const { return this->power_factor_sensor_; }
  void set_power_split_line_a_sensor(sensor::Sensor *sensor) { this->power_split_line_a_sensor_ = sensor; }
  sensor::Sensor *get_power_split_line_a_sensor() const { return this->power_split_line_a_sensor_; }
  void set_power_split_line_b_sensor(sensor::Sensor *sensor) { this->power_split_line_b_sensor_ = sensor; }
  sensor::Sensor *get_power_split_line_b_sensor() const { return this->power_split_line_b_sensor_; }
  void add_power_multiply_filter(float multiplier) { this->power_filters_.add_multiply_filter(multiplier); }
  void add_power_lambda_filter(std::function<float(float)> filter) {
    this->power_filters_.add_lambda_filter(std::move(filter));
  }
  float apply_power_filters(float value) const { return this->power_filters_.apply(value); }
  size_t get_power_filter_count() const { return this->power_filters_.size(); }
  void set_phase_detection_sensor(text_sensor::TextSensor *sensor) { this->phase_detection_sensor_ = sensor; }
  text_sensor::TextSensor *get_phase_detection_sensor() const { return this->phase_detection_sensor_; }
  void set_phase_detection_name(const std::string &name) { this->phase_detection_name_ = name; }
  const std::string &get_phase_detection_name() const { return this->phase_detection_name_; }
  void set_phase_detection_power_min(float power_min) { this->phase_detection_power_min_ = power_min; }
  float get_phase_detection_power_min() const { return this->phase_detection_power_min_; }
  void add_phase_detection_candidate(MeteringPhaseConfig *phase, uint8_t line) {
    this->phase_detection_candidates_.push_back(PhaseDetectionCandidate{phase, line});
  }
  const std::vector<PhaseDetectionCandidate> &get_phase_detection_candidates() const {
    return this->phase_detection_candidates_;
  }
  void reset_phase_detection() {
    this->phase_detection_scores_.fill(0.0f);
    this->phase_detection_samples_ = 0;
  }
  void reset_phase_detection_stability() {
    this->phase_detection_candidate_line_ = 0;
    this->phase_detection_candidate_windows_ = 0;
  }
  uint8_t update_phase_detection_candidate(uint8_t line) {
    if (line < 1 || line > 3) {
      this->reset_phase_detection_stability();
      return 0;
    }
    if (this->phase_detection_candidate_line_ == line) {
      if (this->phase_detection_candidate_windows_ < UINT8_MAX) {
        this->phase_detection_candidate_windows_++;
      }
    } else {
      this->phase_detection_candidate_line_ = line;
      this->phase_detection_candidate_windows_ = 1;
    }
    return this->phase_detection_candidate_windows_;
  }
  void add_phase_detection_score(uint8_t line, float score) {
    if (line >= 1 && line <= 3) {
      this->phase_detection_scores_[line - 1] += score;
    }
  }
  const std::array<float, 3> &get_phase_detection_scores() const { return this->phase_detection_scores_; }
  void increment_phase_detection_samples() { this->phase_detection_samples_++; }
  uint32_t get_phase_detection_samples() const { return this->phase_detection_samples_; }
  void set_phase_detection_window_start_ms(uint32_t value) { this->phase_detection_window_start_ms_ = value; }
  uint32_t get_phase_detection_window_start_ms() const { return this->phase_detection_window_start_ms_; }

 protected:
  MeteringPhaseConfig *phase_{nullptr};
  MeteringPhaseConfig *line_pair_phase_b_{nullptr};
  bool line_pair_{false};
  uint8_t input_port_{0};
  std::vector<MeteringPowerOutput> power_outputs_{};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *apparent_power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  sensor::Sensor *power_split_line_a_sensor_{nullptr};
  sensor::Sensor *power_split_line_b_sensor_{nullptr};
  MeteringPowerFilters power_filters_{};
  text_sensor::TextSensor *phase_detection_sensor_{nullptr};
  std::string phase_detection_name_{};
  float phase_detection_power_min_{30.0f};
  std::vector<PhaseDetectionCandidate> phase_detection_candidates_{};
  std::array<float, 3> phase_detection_scores_{0.0f, 0.0f, 0.0f};
  uint32_t phase_detection_samples_{0};
  uint32_t phase_detection_window_start_ms_{0};
  uint8_t phase_detection_candidate_line_{0};
  uint8_t phase_detection_candidate_windows_{0};
};

class MeteringGroupConfig {
 public:
  struct Term {
    float sign{1.0f};
    MeteringCTClampConfig *ct_clamp{nullptr};
    MeteringGroupConfig *group{nullptr};
  };

  void add_ct_clamp_term(MeteringCTClampConfig *ct_clamp, float sign) {
    this->terms_.push_back(Term{sign, ct_clamp, nullptr});
  }
  void add_group_term(MeteringGroupConfig *group, float sign) {
    this->terms_.push_back(Term{sign, nullptr, group});
  }
  const std::vector<Term> &get_terms() const { return this->terms_; }
  void add_power_output(uint8_t direction, sensor::Sensor *raw_power_sensor, sensor::Sensor *power_sensor) {
    this->power_outputs_.emplace_back(direction, raw_power_sensor, power_sensor);
  }
  const std::vector<MeteringPowerOutput> &get_power_outputs() const { return this->power_outputs_; }
  void add_power_multiply_filter(float multiplier) { this->power_filters_.add_multiply_filter(multiplier); }
  void add_power_lambda_filter(std::function<float(float)> filter) {
    this->power_filters_.add_lambda_filter(std::move(filter));
  }
  float apply_power_filters(float value) const { return this->power_filters_.apply(value); }
  size_t get_power_filter_count() const { return this->power_filters_.size(); }

 protected:
  std::vector<Term> terms_{};
  std::vector<MeteringPowerOutput> power_outputs_{};
  MeteringPowerFilters power_filters_{};
};

class MeteringVirtualLineConfig {
 public:
  void set_lines(MeteringPhaseConfig *line_a, MeteringPhaseConfig *line_b) {
    this->line_a_ = line_a;
    this->line_b_ = line_b;
  }
  const MeteringPhaseConfig *get_line_a() const { return this->line_a_; }
  const MeteringPhaseConfig *get_line_b() const { return this->line_b_; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  sensor::Sensor *get_voltage_sensor() const { return this->voltage_sensor_; }

 protected:
  MeteringPhaseConfig *line_a_{nullptr};
  MeteringPhaseConfig *line_b_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
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
