#include "emporiavue.h"
#include "external_samd_firmware.h"
#include "samd09_firmware.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>

namespace esphome {
namespace emporiavue {

void EmporiaVueComponent::setup() {
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
  ESP_LOGCONFIG(TAG, "EmporiaVue SAMD09 SWD reader:");
  LOG_PIN("  SWDIO Pin: ", this->swdio_pin_);
  LOG_PIN("  SWCLK Pin: ", this->swclk_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Runtime mode: %s", this->runtime_mode_ == RuntimeMode::SPI ? "spi" : "i2c");
  if (this->runtime_mode_ == RuntimeMode::SPI) {
    ESP_LOGCONFIG(TAG, "  SPI CLK Pin: GPIO%u", static_cast<unsigned>(this->spi_clk_pin_));
    ESP_LOGCONFIG(TAG, "  SPI DATA Pin: GPIO%u", static_cast<unsigned>(this->spi_data_pin_));
    ESP_LOGCONFIG(TAG, "  SPI FRAME Pin: GPIO%u", static_cast<unsigned>(this->spi_frame_pin_));
    ESP_LOGCONFIG(TAG, "  SPI main current delay: %u scans", static_cast<unsigned>(this->spi_main_current_delay_));
    ESP_LOGCONFIG(TAG, "  SPI mux current delay: %u scans", static_cast<unsigned>(this->spi_mux_current_delay_));
  }
  ESP_LOGCONFIG(TAG, "  Connect under reset: %s", YESNO(this->connect_under_reset_));
  ESP_LOGCONFIG(TAG, "  SWD on boot: %s", YESNO(this->swd_on_boot_));
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
  ESP_LOGCONFIG(TAG, "  External SAMD firmware images: %u", static_cast<unsigned>(EXTERNAL_SAMD_FIRMWARE_COUNT));
  for (uint8_t i = 0; i < EXTERNAL_SAMD_FIRMWARE_COUNT; i++) {
    ESP_LOGCONFIG(TAG, "    External image %u size: %" PRIu32 " bytes", static_cast<unsigned>(i),
                  this->external_firmware_size_(i));
  }
  ESP_LOGCONFIG(TAG, "  Auto-update SAMD: %s", YESNO(this->auto_update_samd_));
  if (this->diagnostics_interval_ms_ == 0) {
    ESP_LOGCONFIG(TAG, "  Diagnostics interval: disabled");
  } else {
    ESP_LOGCONFIG(TAG, "  Diagnostics interval: %" PRIu32 " ms", this->diagnostics_interval_ms_);
  }
  if (this->metering_interval_ms_ == 0) {
    ESP_LOGCONFIG(TAG, "  Metering interval: disabled");
  } else {
    ESP_LOGCONFIG(TAG, "  Metering interval: %" PRIu32 " ms", this->metering_interval_ms_);
  }
  ESP_LOGCONFIG(TAG, "  Minimum apparent power: %.1f VA", this->minimum_apparent_power_);
  ESP_LOGCONFIG(TAG, "  Minimum fundamental current: %.3f A", this->minimum_fundamental_current_);
  ESP_LOGCONFIG(TAG, "  Phase detection confidence ratio: %.2f", this->phase_detection_confidence_ratio_);
  ESP_LOGCONFIG(TAG, "  Phase detection window: %" PRIu32 " ms", this->phase_detection_update_interval_ms_);
  const char *entity_prefix = this->entity_prefix_.empty() ? "(default)" : this->entity_prefix_.c_str();
  ESP_LOGCONFIG(TAG, "  Entity prefix: %s", entity_prefix);
  LOG_TEXT_SENSOR("  ", "Firmware version", this->firmware_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Bundled firmware version", this->bundled_firmware_version_sensor_);
  for (auto *phase : this->metering_phases_) {
    ESP_LOGCONFIG(TAG, "  Metering phase");
    ESP_LOGCONFIG(TAG, "    Input: %u", static_cast<unsigned>(phase->get_input_wire()));
    ESP_LOGCONFIG(TAG, "    Voltage calibration: %.6f", phase->get_calibration());
    LOG_NUMBER("    ", "Voltage calibration", phase->get_calibration_number());
    LOG_SENSOR("    ", "Voltage", phase->get_voltage_sensor());
    LOG_SENSOR("    ", "Frequency", phase->get_frequency_sensor());
    LOG_SENSOR("    ", "Phase angle", phase->get_phase_angle_sensor());
    LOG_SENSOR("    ", "Voltage THD", phase->get_voltage_thd_sensor());
  }
  for (auto *ct_clamp : this->metering_ct_clamps_) {
    ESP_LOGCONFIG(TAG, "  Metering CT clamp");
    ESP_LOGCONFIG(TAG, "    Input port: %u", static_cast<unsigned>(ct_clamp->get_input_port()));
    if (ct_clamp->is_line_pair()) {
      const auto *phase_a = ct_clamp->get_phase();
      const auto *phase_b = ct_clamp->get_line_pair_phase_b();
      if (phase_a != nullptr && phase_b != nullptr) {
        ESP_LOGCONFIG(TAG, "    Voltage reference: input %u - input %u",
                      static_cast<unsigned>(phase_a->get_input_wire()),
                      static_cast<unsigned>(phase_b->get_input_wire()));
      }
    } else if (ct_clamp->get_phase() != nullptr) {
      ESP_LOGCONFIG(TAG, "    Voltage reference: input %u",
                    static_cast<unsigned>(ct_clamp->get_phase()->get_input_wire()));
    }
    ESP_LOGCONFIG(TAG, "    Internal power filters: %u", static_cast<unsigned>(ct_clamp->get_power_filter_count()));
    LOG_SELECT("    ", "Line", ct_clamp->get_line_select());
    ESP_LOGCONFIG(TAG, "    Automatic line detection: %s", YESNO(ct_clamp->is_auto_line_detection_active()));
    ESP_LOGCONFIG(TAG, "    Current gain: %.6f", ct_clamp->get_current_gain());
    ESP_LOGCONFIG(TAG, "    Current phase correction: %.3f deg", ct_clamp->get_current_phase_correction());
    LOG_NUMBER("    ", "Current gain calibration", ct_clamp->get_current_gain_number());
    LOG_NUMBER("    ", "Current phase calibration", ct_clamp->get_current_phase_number());
    ESP_LOGCONFIG(TAG, "    Power outputs: %u", static_cast<unsigned>(ct_clamp->get_power_outputs().size()));
    for (const auto &output : ct_clamp->get_power_outputs()) {
      LOG_SENSOR("      ", "Raw power", output.get_raw_power_sensor());
      LOG_SENSOR("      ", "Power", output.get_power_sensor());
    }
    LOG_SENSOR("    ", "Current", ct_clamp->get_current_sensor());
    LOG_SENSOR("    ", "Apparent power", ct_clamp->get_apparent_power_sensor());
    LOG_SENSOR("    ", "Power factor", ct_clamp->get_power_factor_sensor());
    if (ct_clamp->has_demand()) {
      ESP_LOGCONFIG(TAG, "    Demand interval: %.0f min", ct_clamp->get_demand_interval() / 60000.0f);
      LOG_SENSOR("    ", "Power demand", ct_clamp->get_power_demand_sensor());
      LOG_SENSOR("    ", "Today's maximum power demand", ct_clamp->get_maximum_power_demand_sensor());
      LOG_SENSOR("    ", "Current demand", ct_clamp->get_current_demand_sensor());
      LOG_SENSOR("    ", "Today's maximum current demand", ct_clamp->get_maximum_current_demand_sensor());
    }
    if (ct_clamp->has_peak_analysis()) {
      ESP_LOGCONFIG(TAG, "    Peak interval: %.1f s", ct_clamp->get_peak_interval() / 1000.0f);
      LOG_SENSOR("    ", "Current peak", ct_clamp->get_current_peak_sensor());
      LOG_SENSOR("    ", "Current crest factor", ct_clamp->get_current_crest_factor_sensor());
    }
    LOG_SENSOR("    ", "Fundamental current", ct_clamp->get_fundamental_current_sensor());
    LOG_SENSOR("    ", "Fundamental reactive power", ct_clamp->get_fundamental_reactive_power_sensor());
    LOG_SENSOR("    ", "Fundamental power factor", ct_clamp->get_fundamental_power_factor_sensor());
    LOG_SENSOR("    ", "Displacement angle", ct_clamp->get_displacement_angle_sensor());
    LOG_SENSOR("    ", "Current THD", ct_clamp->get_current_thd_sensor());
    LOG_SENSOR("    ", "Power split line A", ct_clamp->get_power_split_line_a_sensor());
    LOG_SENSOR("    ", "Power split line B", ct_clamp->get_power_split_line_b_sensor());
    LOG_TEXT_SENSOR("    ", "Phase detection", ct_clamp->get_phase_detection_sensor());
  }
  for (auto *group : this->metering_groups_) {
    ESP_LOGCONFIG(TAG, "  Metering group");
    ESP_LOGCONFIG(TAG, "    Term count: %u", static_cast<unsigned>(group->get_terms().size()));
    ESP_LOGCONFIG(TAG, "    Internal power filters: %u", static_cast<unsigned>(group->get_power_filter_count()));
    ESP_LOGCONFIG(TAG, "    Power outputs: %u", static_cast<unsigned>(group->get_power_outputs().size()));
    for (const auto &output : group->get_power_outputs()) {
      LOG_SENSOR("      ", "Raw power", output.get_raw_power_sensor());
      LOG_SENSOR("      ", "Power", output.get_power_sensor());
    }
    if (group->has_power_demand()) {
      ESP_LOGCONFIG(TAG, "    Demand interval: %.0f min", group->get_demand_interval() / 60000.0f);
      LOG_SENSOR("    ", "Power demand", group->get_power_demand_sensor());
      LOG_SENSOR("    ", "Today's maximum power demand", group->get_maximum_power_demand_sensor());
    }
  }
  for (auto *virtual_line : this->metering_virtual_lines_) {
    ESP_LOGCONFIG(TAG, "  Metering virtual line");
    const auto *line_a = virtual_line->get_line_a();
    const auto *line_b = virtual_line->get_line_b();
    if (line_a != nullptr && line_b != nullptr) {
      ESP_LOGCONFIG(TAG, "    Voltage reference: input %u - input %u",
                    static_cast<unsigned>(line_a->get_input_wire()),
                    static_cast<unsigned>(line_b->get_input_wire()));
    }
    LOG_SENSOR("    ", "Voltage", virtual_line->get_voltage_sensor());
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
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
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
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
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
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
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
    this->stop_spi_receiver_(true);
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
