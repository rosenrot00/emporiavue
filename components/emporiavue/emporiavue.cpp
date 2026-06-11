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
  this->publish_initial_firmware_detection_();
  for (auto *phase : this->metering_phases_) {
    phase->setup_calibration_number();
  }
  if (!this->install_active_) {
    this->start_i2c_diagnostics_();
    this->start_metering_();
    this->setup_spi_receiver_();
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
  this->process_spi_receiver_();
}

void EmporiaVueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EmporiaVue SAMD09 SWD reader:");
  LOG_PIN("  SWDIO Pin: ", this->swdio_pin_);
  LOG_PIN("  SWCLK Pin: ", this->swclk_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  SPI CLK Pin: GPIO%u", static_cast<unsigned>(this->spi_clk_pin_));
  ESP_LOGCONFIG(TAG, "  SPI DATA Pin: GPIO%u", static_cast<unsigned>(this->spi_data_pin_));
  ESP_LOGCONFIG(TAG, "  SPI FRAME Pin: GPIO%u", static_cast<unsigned>(this->spi_frame_pin_));
  ESP_LOGCONFIG(TAG, "  SPI main current delay: %u scans", static_cast<unsigned>(this->spi_main_current_delay_));
  ESP_LOGCONFIG(TAG, "  SPI mux current delay: %u scans", static_cast<unsigned>(this->spi_mux_current_delay_));
  ESP_LOGCONFIG(TAG, "  Connect under reset: %s", YESNO(this->connect_under_reset_));
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
  ESP_LOGCONFIG(TAG, "  Runtime mode: %s", this->runtime_mode_ == RuntimeMode::SPI ? "spi" : "i2c");
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
  ESP_LOGCONFIG(TAG, "  Apparent power minimum: %.1f VA", this->power_apparent_min_);
  ESP_LOGCONFIG(TAG, "  Phase detection confidence ratio: %.2f", this->phase_detection_confidence_ratio_);
  ESP_LOGCONFIG(TAG, "  Phase detection window: %" PRIu32 " ms", this->phase_detection_update_interval_ms_);
  const char *entity_prefix = this->entity_prefix_.empty() ? "(default)" : this->entity_prefix_.c_str();
  ESP_LOGCONFIG(TAG, "  Entity prefix: %s", entity_prefix);
  LOG_TEXT_SENSOR("  ", "Firmware version", this->firmware_version_sensor_);
  LOG_TEXT_SENSOR("  ", "Bundled firmware version", this->bundled_firmware_version_sensor_);
  for (auto *phase : this->metering_phases_) {
    ESP_LOGCONFIG(TAG, "  Metering phase");
    ESP_LOGCONFIG(TAG, "    Input: %u", static_cast<unsigned>(phase->get_input_wire()));
    ESP_LOGCONFIG(TAG, "    Calibration: %.6f", phase->get_calibration());
    LOG_NUMBER("    ", "Calibration", phase->get_calibration_number());
    LOG_SENSOR("    ", "Voltage", phase->get_voltage_sensor());
    LOG_SENSOR("    ", "Frequency", phase->get_frequency_sensor());
    LOG_SENSOR("    ", "Phase angle", phase->get_phase_angle_sensor());
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
    ESP_LOGCONFIG(TAG, "    Power outputs: %u", static_cast<unsigned>(ct_clamp->get_power_outputs().size()));
    for (const auto &output : ct_clamp->get_power_outputs()) {
      LOG_SENSOR("      ", "Raw power", output.get_raw_power_sensor());
      LOG_SENSOR("      ", "Power", output.get_power_sensor());
    }
    LOG_SENSOR("    ", "Current", ct_clamp->get_current_sensor());
    LOG_SENSOR("    ", "Apparent power", ct_clamp->get_apparent_power_sensor());
    LOG_SENSOR("    ", "Power factor", ct_clamp->get_power_factor_sensor());
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
