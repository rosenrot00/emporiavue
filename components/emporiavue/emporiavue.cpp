#include "emporiavue.h"
#include "external_samd_firmware.h"
#include "samd09_firmware.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_system.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace emporiavue {

namespace {

void append_config_item(char *buffer, size_t buffer_size, const char *item) {
  if (buffer_size == 0) {
    return;
  }
  const size_t used = std::strlen(buffer);
  if (used >= buffer_size - 1) {
    return;
  }
  std::snprintf(buffer + used, buffer_size - used, "%s%s", used == 0 ? "" : ",", item);
}

}  // namespace

void EmporiaVueComponent::setup() {
  this->publish_restart_reason_();
  this->inspect_backup_partition_();
  this->publish_bundled_firmware_version_();
  if (this->swd_on_boot_) {
    this->publish_initial_firmware_detection_();
  } else {
    ESP_LOGI(TAG, "SAMD09 SWD firmware detection and reset skipped at boot");
    FirmwareInfo unknown_info{};
    this->detected_firmware_info_ = unknown_info;
    this->detected_firmware_info_valid_ = false;
    this->publish_firmware_version_(unknown_info);
    if (this->auto_update_samd_) {
      ESP_LOGW(TAG, "SAMD09 auto-update skipped because swd_on_boot is disabled");
    }
  }
  for (auto *phase : this->metering_phases_) {
    phase->setup_calibration_number();
  }
  for (auto *ct_clamp : this->metering_ct_clamps_) {
    ct_clamp->setup_current_calibration_numbers();
    ct_clamp->setup_line_assignment();
    ct_clamp->setup_demand();
  }
  for (auto *group : this->metering_groups_) {
    group->setup_demand();
  }
  if (!this->install_active_) {
    this->start_i2c_diagnostics_();
    this->start_metering_();
    this->setup_spi_receiver_();
  }
}

const char *EmporiaVueComponent::restart_reason_name_(uint32_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "power-on event";
    case ESP_RST_EXT:
      return "external pin";
    case ESP_RST_SW:
      return "software restart";
    case ESP_RST_PANIC:
      return "exception/panic";
    case ESP_RST_INT_WDT:
      return "interrupt watchdog";
    case ESP_RST_TASK_WDT:
      return "task watchdog";
    case ESP_RST_WDT:
      return "other watchdog";
    case ESP_RST_DEEPSLEEP:
      return "exiting deep sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "SDIO reset";
    case ESP_RST_USB:
      return "USB reset";
    case ESP_RST_JTAG:
      return "JTAG reset";
    case ESP_RST_EFUSE:
      return "eFuse error";
    case ESP_RST_PWR_GLITCH:
      return "power glitch";
    case ESP_RST_CPU_LOCKUP:
      return "CPU lockup";
    case ESP_RST_UNKNOWN:
    default:
      return "unknown source";
  }
}

void EmporiaVueComponent::publish_restart_reason_() {
#ifdef USE_ESP32
  const uint32_t reason = static_cast<uint32_t>(esp_reset_reason());
  const char *reason_name = restart_reason_name_(reason);
  ESP_LOGI(TAG, "ESP32 restart reason: %s (%" PRIu32 ")", reason_name, reason);
  if (this->diag_restart_reason_sensor_ != nullptr) {
    this->diag_restart_reason_sensor_->publish_state(reason_name);
  }
#endif
}

void EmporiaVueComponent::loop() {
  const uint32_t now = millis();
  if ((now - this->last_peak_check_ms_) >= 50) {
    this->last_peak_check_ms_ = now;
    for (auto *ct_clamp : this->metering_ct_clamps_) {
      ct_clamp->loop_peak(now);
    }
  }
  if ((now - this->last_demand_day_check_ms_) >= 1000) {
    this->last_demand_day_check_ms_ = now;
    for (auto *ct_clamp : this->metering_ct_clamps_) {
      ct_clamp->loop_demand();
    }
    for (auto *group : this->metering_groups_) {
      group->loop_demand();
    }
  }
  if (this->backup_active_) {
    this->process_backup_();
    return;
  }
  if (this->install_active_) {
    this->process_install_();
    return;
  }
  this->process_spi_receiver_();
}

void EmporiaVueComponent::dump_config() {
  const char *runtime_mode = this->runtime_mode_ == RuntimeMode::SPI ? "spi" : "i2c";
  ESP_LOGCONFIG(TAG, "Emporia Vue:");
  ESP_LOGCONFIG(TAG, "  Hardware ID: %u, runtime: %s", static_cast<unsigned>(this->hardware_id_), runtime_mode);

  if (this->detected_firmware_info_valid_) {
    const auto &firmware = this->detected_firmware_info_;
    if (firmware.kind == FirmwareKind::MANAGED) {
      ESP_LOGCONFIG(TAG, "  SAMD firmware: managed v%s (%s), hardware ID %u",
                    format_firmware_version_(firmware.version).c_str(), firmware_mode_name_(firmware.mode_id),
                    static_cast<unsigned>(firmware.hardware_id));
    } else if (firmware.kind == FirmwareKind::STOCK) {
      ESP_LOGCONFIG(TAG, "  SAMD firmware: stock");
    } else {
      ESP_LOGCONFIG(TAG, "  SAMD firmware: unknown");
    }
  } else {
    ESP_LOGCONFIG(TAG, "  SAMD firmware: not detected");
  }

  if (this->bundled_firmware_available_()) {
    ESP_LOGCONFIG(TAG, "  Bundled SAMD: v%s (%s), hardware ID %" PRIu32 ", compatible: %s",
                  format_firmware_version_(this->bundled_firmware_version_()).c_str(),
                  firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_())),
                  this->bundled_firmware_hardware_id_(), YESNO(this->bundled_firmware_matches_target_()));
  } else {
    ESP_LOGCONFIG(TAG, "  Bundled SAMD: none");
  }

  char swdio_summary[GPIO_SUMMARY_MAX_LEN];
  char swclk_summary[GPIO_SUMMARY_MAX_LEN];
  char reset_summary[GPIO_SUMMARY_MAX_LEN];
  this->swdio_pin_->dump_summary(swdio_summary, sizeof(swdio_summary));
  this->swclk_pin_->dump_summary(swclk_summary, sizeof(swclk_summary));
  this->reset_pin_->dump_summary(reset_summary, sizeof(reset_summary));
  ESP_LOGCONFIG(TAG, "  SWD pins: IO=%s, CLK=%s, RESET=%s", swdio_summary, swclk_summary, reset_summary);
  ESP_LOGCONFIG(TAG, "  SWD: on_boot=%s, connect_under_reset=%s, release=%" PRIu32 " ms, clock_delay=%u us",
                YESNO(this->swd_on_boot_), YESNO(this->connect_under_reset_), this->reset_release_time_ms_,
                this->clock_delay_us_);
  if (this->runtime_mode_ == RuntimeMode::SPI) {
    ESP_LOGCONFIG(TAG, "  SPI pins: CLK=GPIO%u, DATA=GPIO%u, FRAME=GPIO%u; current delays: main=%u, mux=%u scans",
                  static_cast<unsigned>(this->spi_clk_pin_), static_cast<unsigned>(this->spi_data_pin_),
                  static_cast<unsigned>(this->spi_frame_pin_), static_cast<unsigned>(this->spi_main_current_delay_),
                  static_cast<unsigned>(this->spi_mux_current_delay_));
  }
  ESP_LOGCONFIG(TAG, "  Firmware management: backup=%s, auto_update=%s, external_images=%u",
                this->backup_partition_name_.c_str(), YESNO(this->auto_update_samd_),
                static_cast<unsigned>(EXTERNAL_SAMD_FIRMWARE_COUNT));
  ESP_LOGCONFIG(TAG, "  Intervals: metering=%" PRIu32 " ms, diagnostics=%" PRIu32 " ms",
                this->metering_interval_ms_, this->diagnostics_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Analysis validity: apparent>=%.1f VA, fundamental_current>=%.3f A",
                this->minimum_apparent_power_, this->minimum_fundamental_current_);
  ESP_LOGCONFIG(TAG, "  Phase detection window: %" PRIu32 " ms", this->phase_detection_update_interval_ms_);
  if (!this->entity_prefix_.empty()) {
    ESP_LOGCONFIG(TAG, "  Entity prefix: %s", this->entity_prefix_.c_str());
  }

  ESP_LOGCONFIG(TAG, "  Topology: %u phases, %u CTs, %u groups, %u virtual lines",
                static_cast<unsigned>(this->metering_phases_.size()),
                static_cast<unsigned>(this->metering_ct_clamps_.size()),
                static_cast<unsigned>(this->metering_groups_.size()),
                static_cast<unsigned>(this->metering_virtual_lines_.size()));

  for (auto *phase : this->metering_phases_) {
    char features[96]{};
    if (phase->get_voltage_sensor() != nullptr)
      append_config_item(features, sizeof(features), "voltage");
    if (phase->get_frequency_sensor() != nullptr)
      append_config_item(features, sizeof(features), "frequency");
    if (phase->get_phase_angle_sensor() != nullptr)
      append_config_item(features, sizeof(features), "phase_angle");
    if (phase->get_voltage_thd_sensor() != nullptr)
      append_config_item(features, sizeof(features), "voltage_thd");
    if (phase->get_calibration_number() != nullptr)
      append_config_item(features, sizeof(features), "calibration_control");
    ESP_LOGCONFIG(TAG, "  Phase %s: input=%u, calibration=%.6f, entities=%s", phase->get_config_key(),
                  static_cast<unsigned>(phase->get_input_wire()), phase->get_calibration(),
                  features[0] == '\0' ? "none" : features);
  }

  for (auto *ct_clamp : this->metering_ct_clamps_) {
    char voltage_reference[32] = "unassigned";
    if (ct_clamp->is_line_pair()) {
      const auto *phase_a = ct_clamp->get_phase();
      const auto *phase_b = ct_clamp->get_line_pair_phase_b();
      if (phase_a != nullptr && phase_b != nullptr) {
        std::snprintf(voltage_reference, sizeof(voltage_reference), "input %u-input %u",
                      static_cast<unsigned>(phase_a->get_input_wire()), static_cast<unsigned>(phase_b->get_input_wire()));
      }
    } else if (ct_clamp->get_phase() != nullptr) {
      std::snprintf(voltage_reference, sizeof(voltage_reference), "input %u",
                    static_cast<unsigned>(ct_clamp->get_phase()->get_input_wire()));
    }

    char settings[128]{};
    if (ct_clamp->get_power_filter_count() != 0) {
      char value[24];
      std::snprintf(value, sizeof(value), "filters=%u", static_cast<unsigned>(ct_clamp->get_power_filter_count()));
      append_config_item(settings, sizeof(settings), value);
    }
    if (ct_clamp->get_line_select() != nullptr)
      append_config_item(settings, sizeof(settings), "line_select");
    if (ct_clamp->is_auto_line_detection_active())
      append_config_item(settings, sizeof(settings), "auto_line");
    if (!ct_clamp->get_phase_detection_candidates().empty())
      append_config_item(settings, sizeof(settings),
                         ct_clamp->is_phase_detection_export() ? "phase_detection=export"
                                                               : "phase_detection=import");
    if (ct_clamp->get_current_gain_number() != nullptr || ct_clamp->get_current_phase_number() != nullptr ||
        std::fabs(ct_clamp->get_current_gain() - 1.0f) > 0.000001f ||
        std::fabs(ct_clamp->get_current_phase_correction()) > 0.0001f) {
      char value[64];
      std::snprintf(value, sizeof(value), "gain=%.6f,phase=%.3fdeg", ct_clamp->get_current_gain(),
                    ct_clamp->get_current_phase_correction());
      append_config_item(settings, sizeof(settings), value);
    }

    char features[320]{};
    if (!ct_clamp->get_power_outputs().empty())
      append_config_item(features, sizeof(features), "power");
    if (ct_clamp->get_current_sensor() != nullptr)
      append_config_item(features, sizeof(features), "current");
    if (ct_clamp->get_apparent_power_sensor() != nullptr)
      append_config_item(features, sizeof(features), "apparent_power");
    if (ct_clamp->get_power_factor_sensor() != nullptr)
      append_config_item(features, sizeof(features), "power_factor");
    if (ct_clamp->get_power_demand_sensor() != nullptr)
      append_config_item(features, sizeof(features), "power_demand");
    if (ct_clamp->get_maximum_power_demand_sensor() != nullptr)
      append_config_item(features, sizeof(features), "max_power_demand");
    if (ct_clamp->get_current_demand_sensor() != nullptr)
      append_config_item(features, sizeof(features), "current_demand");
    if (ct_clamp->get_maximum_current_demand_sensor() != nullptr)
      append_config_item(features, sizeof(features), "max_current_demand");
    if (ct_clamp->get_current_peak_sensor() != nullptr)
      append_config_item(features, sizeof(features), "current_peak");
    if (ct_clamp->get_current_crest_factor_sensor() != nullptr)
      append_config_item(features, sizeof(features), "crest_factor");
    if (ct_clamp->get_fundamental_current_sensor() != nullptr)
      append_config_item(features, sizeof(features), "fundamental_current");
    if (ct_clamp->get_fundamental_reactive_power_sensor() != nullptr)
      append_config_item(features, sizeof(features), "fundamental_reactive_power");
    if (ct_clamp->get_fundamental_power_factor_sensor() != nullptr)
      append_config_item(features, sizeof(features), "fundamental_power_factor");
    if (ct_clamp->get_displacement_angle_sensor() != nullptr)
      append_config_item(features, sizeof(features), "displacement_angle");
    if (ct_clamp->get_current_thd_sensor() != nullptr)
      append_config_item(features, sizeof(features), "current_thd");
    if (ct_clamp->get_power_split_line_a_sensor() != nullptr || ct_clamp->get_power_split_line_b_sensor() != nullptr)
      append_config_item(features, sizeof(features), "power_split");
    if (ct_clamp->get_phase_detection_sensor() != nullptr)
      append_config_item(features, sizeof(features), "phase_detection");

    if (settings[0] == '\0') {
      ESP_LOGCONFIG(TAG, "  CT %s: input=%u, voltage=%s, entities=%s", ct_clamp->get_config_key(),
                    static_cast<unsigned>(ct_clamp->get_input_port()), voltage_reference,
                    features[0] == '\0' ? "none" : features);
    } else {
      ESP_LOGCONFIG(TAG, "  CT %s: input=%u, voltage=%s, %s, entities=%s", ct_clamp->get_config_key(),
                    static_cast<unsigned>(ct_clamp->get_input_port()), voltage_reference, settings,
                    features[0] == '\0' ? "none" : features);
    }
  }

  for (const auto *group : this->metering_groups_) {
    char features[64]{};
    if (!group->get_power_outputs().empty())
      append_config_item(features, sizeof(features), "power");
    if (group->get_power_demand_sensor() != nullptr)
      append_config_item(features, sizeof(features), "power_demand");
    if (group->get_maximum_power_demand_sensor() != nullptr)
      append_config_item(features, sizeof(features), "max_power_demand");
    ESP_LOGCONFIG(TAG, "  Group %s: sources=%u, filters=%u, entities=%s", group->get_config_key(),
                  static_cast<unsigned>(group->get_terms().size()),
                  static_cast<unsigned>(group->get_power_filter_count()), features[0] == '\0' ? "none" : features);
  }

  for (const auto *virtual_line : this->metering_virtual_lines_) {
    const auto *line_a = virtual_line->get_line_a();
    const auto *line_b = virtual_line->get_line_b();
    if (line_a != nullptr && line_b != nullptr) {
      ESP_LOGCONFIG(TAG, "  Virtual line %s: input %u-input %u, entity=voltage", virtual_line->get_config_key(),
                    static_cast<unsigned>(line_a->get_input_wire()), static_cast<unsigned>(line_b->get_input_wire()));
    } else {
      ESP_LOGCONFIG(TAG, "  Virtual line %s: unassigned", virtual_line->get_config_key());
    }
  }
}

void MeteringPhaseConfig::setup_calibration_number() {
  if (this->calibration_number_ == nullptr) {
    return;
  }
  this->calibration_number_->setup_value();
}

void MeteringCalibrationNumber::setup_value() {
  float value = this->initial_value_;
  this->ensure_preference_();
  this->pref_.load(&value);
  if (this->parent_ != nullptr) {
    this->parent_->set_calibration(value);
  }
  this->publish_state(value);
}

void MeteringCalibrationNumber::control(float value) {
  if (this->parent_ != nullptr) {
    this->parent_->set_calibration(value);
  }
  this->ensure_preference_();
  this->pref_.save(&value);
  global_preferences->sync();
  this->publish_state(value);
}

void MeteringCalibrationNumber::ensure_preference_() {
  if (this->pref_initialized_) {
    return;
  }
  this->pref_ = global_preferences->make_preference<float>(this->preference_key_);
  this->pref_initialized_ = true;
}

void MeteringCTClampConfig::setup_current_calibration_numbers() {
  if (this->current_gain_number_ != nullptr) {
    this->current_gain_number_->setup_value();
  }
  if (this->current_phase_number_ != nullptr) {
    this->current_phase_number_->setup_value();
  }
}

void MeteringCurrentGainNumber::setup_value() {
  float value = this->initial_value_;
  this->ensure_preference_();
  this->pref_.load(&value);
  if (this->parent_ != nullptr) {
    this->parent_->set_current_gain(value);
  }
  this->publish_state(value);
}

void MeteringCurrentGainNumber::control(float value) {
  if (this->parent_ != nullptr) {
    this->parent_->set_current_gain(value);
  }
  this->ensure_preference_();
  this->pref_.save(&value);
  global_preferences->sync();
  this->publish_state(value);
}

void MeteringCurrentGainNumber::ensure_preference_() {
  if (this->pref_initialized_) {
    return;
  }
  this->pref_ = global_preferences->make_preference<float>(this->preference_key_);
  this->pref_initialized_ = true;
}

void MeteringCurrentPhaseNumber::setup_value() {
  float value = this->initial_value_;
  this->ensure_preference_();
  this->pref_.load(&value);
  if (this->parent_ != nullptr) {
    this->parent_->set_current_phase_correction(value);
  }
  this->publish_state(value);
}

void MeteringCurrentPhaseNumber::control(float value) {
  if (this->parent_ != nullptr) {
    this->parent_->set_current_phase_correction(value);
  }
  this->ensure_preference_();
  this->pref_.save(&value);
  global_preferences->sync();
  this->publish_state(value);
}

void MeteringCurrentPhaseNumber::ensure_preference_() {
  if (this->pref_initialized_) {
    return;
  }
  this->pref_ = global_preferences->make_preference<float>(this->preference_key_);
  this->pref_initialized_ = true;
}

void MeteringLineSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (value == "Auto") {
    this->parent_->start_auto_line_detection();
    return;
  }
  if (value.size() == 2 && value[0] == 'L' && value[1] >= '1' && value[1] <= '3') {
    this->parent_->select_line(static_cast<uint8_t>(value[1] - '0'));
  }
}

void MeteringCTClampConfig::setup_line_assignment() {
  if (!this->dynamic_line_enabled_) {
    return;
  }

  this->line_pref_ = global_preferences->make_preference<uint8_t>(this->line_preference_key_);
  this->line_pref_initialized_ = true;
  uint8_t line = this->initial_line_;
  this->line_pref_.load(&line);
  if (line >= 1 && line <= 3 && this->select_line(line, false)) {
    return;
  }
  this->start_auto_line_detection(false);
}

void MeteringCTClampConfig::save_line_assignment_(uint8_t line) {
  if (!this->dynamic_line_enabled_) {
    return;
  }
  if (!this->line_pref_initialized_) {
    this->line_pref_ = global_preferences->make_preference<uint8_t>(this->line_preference_key_);
    this->line_pref_initialized_ = true;
  }
  this->line_pref_.save(&line);
  global_preferences->sync();
}

void MeteringCTClampConfig::start_auto_line_detection(bool save) {
  if (!this->dynamic_line_enabled_) {
    return;
  }
  this->phase_ = nullptr;
  this->line_pair_phase_b_ = nullptr;
  this->line_pair_ = false;
  this->auto_line_detection_active_ = true;
  this->reset_phase_detection();
  this->reset_phase_detection_reference();
  this->reset_phase_detection_stability();
  this->phase_detection_window_start_ms_ = 0;
  this->power_demand_.invalidate_window();
  if (save) {
    this->save_line_assignment_(0);
  }
  if (this->line_select_ != nullptr) {
    this->line_select_->publish_state("Auto");
  }
}

bool MeteringCTClampConfig::select_line(uint8_t line, bool save) {
  for (const auto &candidate : this->phase_detection_candidates_) {
    if (candidate.line != line || candidate.phase == nullptr) {
      continue;
    }
    this->set_phase(candidate.phase);
    this->auto_line_detection_active_ = false;
    this->reset_phase_detection();
    this->reset_phase_detection_reference();
    this->reset_phase_detection_stability();
    this->phase_detection_window_start_ms_ = 0;
    this->power_demand_.invalidate_window();
    if (save) {
      this->save_line_assignment_(line);
    }
    if (this->line_select_ != nullptr) {
      this->line_select_->publish_state(str_sprintf("L%u", static_cast<unsigned>(line)));
    }
    return true;
  }
  return false;
}

void MeteringCTClampConfig::complete_auto_line_detection(uint8_t line) {
  if (this->auto_line_detection_active_) {
    this->select_line(line);
  }
}

void EmporiaVueComponent::set_error_(const std::string &error) {
  this->last_error_ = error;
  ESP_LOGW(TAG, "%s", error.c_str());
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

void EmporiaVueComponent::publish_bundled_firmware_version_() {
  if (this->bundled_firmware_version_sensor_ == nullptr) {
    return;
  }
  if (!this->bundled_firmware_available_()) {
    this->bundled_firmware_version_sensor_->publish_state("none");
    return;
  }
  this->bundled_firmware_version_sensor_->publish_state(
      str_sprintf("v%s (%s)", format_firmware_version_(this->bundled_firmware_version_()).c_str(),
                  firmware_mode_name_(static_cast<uint16_t>(this->bundled_firmware_mode_id_()))));
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

void EmporiaVueComponent::start_i2c_diagnostics_() {
  if (this->diagnostics_started_) {
    return;
  }

  if (this->runtime_mode_ == RuntimeMode::SPI) {
    if (!this->firmware_mode_matches_runtime_()) {
      this->start_firmware_mode_mismatch_log_();
      return;
    }
    this->diagnostics_started_ = true;
    this->stop_firmware_mode_mismatch_log_();
    this->setup_spi_receiver_();
    return;
  }

  if (this->diagnostics_interval_ms_ == 0) {
    this->diagnostics_started_ = true;
    return;
  }
  if (this->runtime_mode_ != RuntimeMode::I2C) {
    this->diagnostics_started_ = true;
    return;
  }
  if (!this->firmware_mode_matches_runtime_()) {
    this->start_firmware_mode_mismatch_log_();
    return;
  }

  this->stop_firmware_mode_mismatch_log_();
  this->diagnostics_started_ = true;
  this->set_interval("samd_i2c_diagnostics", this->diagnostics_interval_ms_,
                     [this]() { this->refresh_i2c_diagnostics_(); });
}

void EmporiaVueComponent::stop_i2c_diagnostics_() {
  this->cancel_timeout("post_update_i2c_probe");
  this->stop_firmware_mode_mismatch_log_();
  if (this->runtime_mode_ == RuntimeMode::SPI) {
    this->stop_spi_receiver_();
    this->diagnostics_started_ = false;
    return;
  }
  if (!this->diagnostics_started_) {
    return;
  }
  this->cancel_interval("samd_i2c_diagnostics");
  this->diagnostics_started_ = false;
}


}  // namespace emporiavue
}  // namespace esphome
