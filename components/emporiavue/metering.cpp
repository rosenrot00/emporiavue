#include "emporiavue.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <string>

namespace esphome {
namespace emporiavue {

void EmporiaVueComponent::submit_metering_frame_(const MeteringFrame &frame) {
  if (!frame.valid) {
    return;
  }

  if (this->last_metering_sequence_valid_ && frame.transport == MeteringTransport::I2C &&
      this->last_metering_transport_ == MeteringTransport::I2C) {
    const uint8_t sequence_delta = static_cast<uint8_t>(frame.sequence - this->last_metering_sequence_);
    if (sequence_delta > 1) {
      ESP_LOGD(TAG, "SAMD09 metering detected %u missing reading(s): previous seq=%" PRIu32 " current seq=%" PRIu32,
               static_cast<unsigned>(static_cast<uint8_t>(sequence_delta - 1)), this->last_metering_sequence_,
               frame.sequence);
    }
  }

  this->last_metering_sequence_ = frame.sequence;
  this->last_metering_transport_ = frame.transport;
  this->last_metering_sequence_valid_ = true;
  this->last_metering_frame_ = frame;

  const char *transport = frame.transport == MeteringTransport::SPI ? "SPI" :
                          frame.transport == MeteringTransport::I2C ? "I2C" : "unknown";
  ESP_LOGV(TAG, "SAMD09 %s metering frame: seq=%" PRIu32 ", flags=0x%02x", transport, frame.sequence,
           frame.quality_flags);
  this->publish_metering_frame_(frame);
}

bool EmporiaVueComponent::calculate_ct_power_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp,
                                              float *power) const {
  if (ct_clamp == nullptr || power == nullptr) {
    return false;
  }

  const uint8_t port = ct_clamp->get_input_port();
  const MeteringPhaseConfig *phase = ct_clamp->get_phase();
  if (port >= 19 || phase == nullptr || phase->get_input_wire() >= 3) {
    return false;
  }

  const float correction_factor = port < 3 ? 5.5f : 22.0f;
  if (ct_clamp->is_line_pair()) {
    const MeteringPhaseConfig *phase_b = ct_clamp->get_line_pair_phase_b();
    if (phase_b == nullptr || phase_b->get_input_wire() >= 3) {
      return false;
    }

    const int32_t raw_power_a = frame.clamps[port].power_raw_by_phase[phase->get_input_wire()];
    const int32_t raw_power_b = frame.clamps[port].power_raw_by_phase[phase_b->get_input_wire()];
    const float power_a = raw_power_a * phase->get_calibration() / correction_factor;
    const float power_b = raw_power_b * phase_b->get_calibration() / correction_factor;
    *power = ct_clamp->apply_power_filters(power_a - power_b);
    return true;
  }

  const int32_t raw_power = frame.clamps[port].power_raw_by_phase[phase->get_input_wire()];
  *power = ct_clamp->apply_power_filters(raw_power * phase->get_calibration() / correction_factor);
  return true;
}

bool EmporiaVueComponent::calculate_phase_voltage_(const MeteringFrame &frame, const MeteringPhaseConfig *phase,
                                                   float *voltage) const {
  if (phase == nullptr || voltage == nullptr || phase->get_input_wire() >= 3) {
    return false;
  }

  *voltage = frame.phases[phase->get_input_wire()].voltage_raw * phase->get_calibration();
  return true;
}

bool EmporiaVueComponent::calculate_line_to_line_voltage_(const MeteringFrame &frame,
                                                          const MeteringPhaseConfig *line_a,
                                                          const MeteringPhaseConfig *line_b, float *voltage) const {
  if (line_a == nullptr || line_b == nullptr || voltage == nullptr || line_a->get_input_wire() >= 3 ||
      line_b->get_input_wire() >= 3) {
    return false;
  }

  float voltage_a = 0.0f;
  float voltage_b = 0.0f;
  if (!this->calculate_phase_voltage_(frame, line_a, &voltage_a) ||
      !this->calculate_phase_voltage_(frame, line_b, &voltage_b)) {
    return false;
  }

  const uint8_t input_a = line_a->get_input_wire();
  const uint8_t input_b = line_b->get_input_wire();
  const float angle_a_degrees = frame.phases[input_a].phase_angle_degrees;
  const float angle_b_degrees = frame.phases[input_b].phase_angle_degrees;
  if (std::isnan(angle_a_degrees) || std::isnan(angle_b_degrees)) {
    return false;
  }

  constexpr float two_pi = 6.28318530717958647692f;
  const float angle_a = angle_a_degrees * two_pi / 360.0f;
  const float angle_b = angle_b_degrees * two_pi / 360.0f;
  const float voltage_sq = voltage_a * voltage_a + voltage_b * voltage_b -
                           2.0f * voltage_a * voltage_b * std::cos(angle_a - angle_b);
  *voltage = std::sqrt(std::max(0.0f, voltage_sq));
  return true;
}

bool EmporiaVueComponent::calculate_ct_voltage_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp,
                                                float *voltage) const {
  if (ct_clamp == nullptr || voltage == nullptr) {
    return false;
  }

  const MeteringPhaseConfig *phase = ct_clamp->get_phase();
  if (!ct_clamp->is_line_pair()) {
    return this->calculate_phase_voltage_(frame, phase, voltage);
  }
  return this->calculate_line_to_line_voltage_(frame, phase, ct_clamp->get_line_pair_phase_b(), voltage);
}

bool EmporiaVueComponent::calculate_ct_current_(const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp,
                                                float *current) const {
  if (ct_clamp == nullptr || current == nullptr) {
    return false;
  }
  const uint8_t port = ct_clamp->get_input_port();
  if (port >= 19) {
    return false;
  }
  const float scalar = port < 3 ? (775.0f / 42624.0f) : (775.0f / 170496.0f);
  *current = frame.clamps[port].current_raw * scalar;
  return true;
}

bool EmporiaVueComponent::calculate_ct_apparent_power_(const MeteringFrame &frame,
                                                       const MeteringCTClampConfig *ct_clamp,
                                                       float *apparent_power) const {
  if (apparent_power == nullptr) {
    return false;
  }

  float voltage = 0.0f;
  float current = 0.0f;
  if (!this->calculate_ct_voltage_(frame, ct_clamp, &voltage) ||
      !this->calculate_ct_current_(frame, ct_clamp, &current)) {
    return false;
  }

  *apparent_power = std::fabs(ct_clamp->apply_power_filters(voltage * current));
  return true;
}

bool EmporiaVueComponent::calculate_ct_measurement_(const MeteringFrame &frame,
                                                    const MeteringCTClampConfig *ct_clamp,
                                                    MeteringCTMeasurement *measurement) const {
  if (ct_clamp == nullptr || measurement == nullptr) {
    return false;
  }

  *measurement = MeteringCTMeasurement{};
  measurement->has_power = this->calculate_ct_power_(frame, ct_clamp, &measurement->power);
  measurement->has_voltage = this->calculate_ct_voltage_(frame, ct_clamp, &measurement->voltage);
  measurement->has_current = this->calculate_ct_current_(frame, ct_clamp, &measurement->current);
  if (measurement->has_voltage && measurement->has_current) {
    measurement->apparent_power =
        std::fabs(ct_clamp->apply_power_filters(measurement->voltage * measurement->current));
    measurement->has_apparent_power = true;
    measurement->has_power_factor = true;
    if (measurement->apparent_power >= this->power_apparent_min_ && measurement->apparent_power > 0.0f &&
        measurement->has_power) {
      measurement->power_factor =
          std::max(0.0f, std::min(1.0f, std::fabs(measurement->power) / measurement->apparent_power));
    }
  }
  return measurement->has_power || measurement->has_voltage || measurement->has_current ||
         measurement->has_apparent_power;
}

float EmporiaVueComponent::apply_power_direction_(float power, uint8_t direction) {
  switch (direction) {
    case MeteringPowerOutput::DIRECTION_POSITIVE:
      return power > 0.0f ? power : 0.0f;
    case MeteringPowerOutput::DIRECTION_NEGATIVE:
      return power < 0.0f ? -power : 0.0f;
    case MeteringPowerOutput::DIRECTION_SIGNED:
    default:
      return power;
  }
}

void EmporiaVueComponent::publish_power_outputs_(const std::vector<MeteringPowerOutput> &outputs, float power) {
  for (const auto &output : outputs) {
    const float directed_power = apply_power_direction_(power, output.get_direction());
    if (output.get_raw_power_sensor() != nullptr) {
      output.get_raw_power_sensor()->publish_state(directed_power);
    }
    if (output.get_power_sensor() != nullptr) {
      output.get_power_sensor()->publish_state(directed_power);
    }
  }
}

bool EmporiaVueComponent::calculate_group_power_(const MeteringFrame &frame, const MeteringGroupConfig *group,
                                                 float *group_power, uint8_t depth) const {
  if (group == nullptr || group_power == nullptr) {
    return false;
  }
  if (depth > 8) {
    return false;
  }

  float sum = 0.0f;
  bool has_power = false;
  for (const auto &term : group->get_terms()) {
    if (term.ct_clamp != nullptr) {
      float power = 0.0f;
      if (this->calculate_ct_power_(frame, term.ct_clamp, &power)) {
        sum += term.sign * power;
        has_power = true;
      }
      continue;
    }

    if (term.group != nullptr) {
      float power = 0.0f;
      if (this->calculate_group_power_(frame, term.group, &power, depth + 1)) {
        sum += term.sign * power;
        has_power = true;
      }
    }
  }
  if (!has_power) {
    return false;
  }

  *group_power = group->apply_power_filters(sum);
  return true;
}

void EmporiaVueComponent::update_phase_detection_(const MeteringFrame &frame, MeteringCTClampConfig *ct_clamp) {
  if (ct_clamp == nullptr || ct_clamp->get_phase_detection_sensor() == nullptr) {
    return;
  }

  const uint8_t port = ct_clamp->get_input_port();
  const auto &candidates = ct_clamp->get_phase_detection_candidates();
  if (port >= 19 || candidates.size() < 2) {
    return;
  }

  const uint32_t now = millis();
  uint32_t window_start = ct_clamp->get_phase_detection_window_start_ms();
  if (window_start == 0) {
    ct_clamp->set_phase_detection_window_start_ms(now);
    window_start = now;
  }

  const float correction_factor = port < 3 ? 5.5f : 22.0f;
  std::array<float, 3> sample_scores{0.0f, 0.0f, 0.0f};
  float max_sample_score = 0.0f;
  uint8_t valid_candidates = 0;

  for (const auto &candidate : candidates) {
    if (candidate.phase == nullptr || candidate.line < 1 || candidate.line > 3) {
      continue;
    }
    const uint8_t input = candidate.phase->get_input_wire();
    if (input >= 3) {
      continue;
    }
    const int32_t raw_power = frame.clamps[port].power_raw_by_phase[input];
    const float power = raw_power * candidate.phase->get_calibration() / correction_factor;
    const float score = std::fabs(power);
    sample_scores[candidate.line - 1] = score;
    max_sample_score = std::max(max_sample_score, score);
    valid_candidates++;
  }
  if (valid_candidates < 2) {
    return;
  }

  if (max_sample_score >= ct_clamp->get_phase_detection_power_min()) {
    for (uint8_t line = 1; line <= 3; line++) {
      ct_clamp->add_phase_detection_score(line, sample_scores[line - 1]);
    }
    ct_clamp->increment_phase_detection_samples();
  }

  if ((now - window_start) < this->phase_detection_update_interval_ms_) {
    return;
  }

  std::string state;
  const uint32_t samples = ct_clamp->get_phase_detection_samples();
  const auto &scores = ct_clamp->get_phase_detection_scores();
  if (samples == 0) {
    state = "low load";
    ct_clamp->reset_phase_detection_stability();
    ct_clamp->get_phase_detection_sensor()->publish_state(state);
    ESP_LOGD(TAG, "%s: low load window=%" PRIu32 "ms power_min=%.1fW",
             ct_clamp->get_phase_detection_name().c_str(), now - window_start,
             ct_clamp->get_phase_detection_power_min());
    ct_clamp->reset_phase_detection();
    ct_clamp->set_phase_detection_window_start_ms(now);
    return;
  }

  uint8_t best_line = 1;
  uint8_t second_line = 2;
  float best_score = -1.0f;
  float second_score = -1.0f;
  for (uint8_t line = 1; line <= 3; line++) {
    const float score = scores[line - 1];
    if (score > best_score) {
      second_score = best_score;
      second_line = best_line;
      best_score = score;
      best_line = line;
    } else if (score > second_score) {
      second_score = score;
      second_line = line;
    }
  }

  const float ratio = second_score > 0.0f ? best_score / second_score : 999.0f;
  const float confidence = (best_score + second_score) > 0.0f ? best_score * 100.0f / (best_score + second_score)
                                                              : 0.0f;
  if (ratio < this->phase_detection_confidence_ratio_) {
    ct_clamp->reset_phase_detection_stability();
    state = str_sprintf("ambiguous L%u/L%u", static_cast<unsigned>(best_line), static_cast<unsigned>(second_line));
  } else {
    const uint8_t stable_windows = ct_clamp->update_phase_detection_candidate(best_line);
    if (stable_windows >= 3) {
      state = str_sprintf("L%u", static_cast<unsigned>(best_line));
    } else {
      state = str_sprintf("L%u weak", static_cast<unsigned>(best_line));
    }
  }

  ct_clamp->get_phase_detection_sensor()->publish_state(state);

  const float divisor = samples == 0 ? 1.0f : static_cast<float>(samples);
  ESP_LOGD(TAG,
           "%s: state=%s window=%" PRIu32 "ms samples=%" PRIu32
           " scores L1=%.1fW L2=%.1fW L3=%.1fW confidence=%.0f%% power_min=%.1fW",
           ct_clamp->get_phase_detection_name().c_str(), state.c_str(), now - window_start, samples,
           scores[0] / divisor, scores[1] / divisor, scores[2] / divisor,
           confidence, ct_clamp->get_phase_detection_power_min());

  ct_clamp->reset_phase_detection();
  ct_clamp->set_phase_detection_window_start_ms(now);
}

void EmporiaVueComponent::publish_metering_frame_(const MeteringFrame &frame) {
  if (!frame.valid) {
    return;
  }

  for (auto *phase : this->metering_phases_) {
    const uint8_t input = phase->get_input_wire();
    if (input >= 3) {
      continue;
    }

    if (phase->get_voltage_sensor() != nullptr) {
      phase->get_voltage_sensor()->publish_state(frame.phases[input].voltage_raw * phase->get_calibration());
    }
    if (phase->get_frequency_sensor() != nullptr && !std::isnan(frame.phases[input].frequency_hz)) {
      phase->get_frequency_sensor()->publish_state(frame.phases[input].frequency_hz);
    }
    if (phase->get_phase_angle_sensor() != nullptr && input > 0 &&
        !std::isnan(frame.phases[input].phase_angle_degrees)) {
      phase->get_phase_angle_sensor()->publish_state(frame.phases[input].phase_angle_degrees);
    }
  }

  for (auto *virtual_line : this->metering_virtual_lines_) {
    if (virtual_line->get_voltage_sensor() == nullptr) {
      continue;
    }
    const MeteringPhaseConfig *line_a = virtual_line->get_line_a();
    const MeteringPhaseConfig *line_b = virtual_line->get_line_b();
    if (line_a == nullptr || line_b == nullptr || line_a->get_input_wire() >= 3 || line_b->get_input_wire() >= 3) {
      continue;
    }
    const uint8_t input_a = line_a->get_input_wire();
    const uint8_t input_b = line_b->get_input_wire();
    if (input_a >= 3 || input_b >= 3) {
      continue;
    }
    float voltage = 0.0f;
    if (this->calculate_line_to_line_voltage_(frame, line_a, line_b, &voltage)) {
      virtual_line->get_voltage_sensor()->publish_state(voltage);
    }
  }

  for (auto *ct_clamp : this->metering_ct_clamps_) {
    const uint8_t port = ct_clamp->get_input_port();
    const MeteringPhaseConfig *phase = ct_clamp->get_phase();
    if (port >= 19 || phase == nullptr || phase->get_input_wire() >= 3) {
      continue;
    }

    MeteringCTMeasurement measurement{};
    this->calculate_ct_measurement_(frame, ct_clamp, &measurement);
    if (measurement.has_power) {
      publish_power_outputs_(ct_clamp->get_power_outputs(), measurement.power);
      if (ct_clamp->is_line_pair()) {
        const float split_power = measurement.power * 0.5f;
        if (ct_clamp->get_power_split_line_a_sensor() != nullptr) {
          ct_clamp->get_power_split_line_a_sensor()->publish_state(split_power);
        }
        if (ct_clamp->get_power_split_line_b_sensor() != nullptr) {
          ct_clamp->get_power_split_line_b_sensor()->publish_state(split_power);
        }
      }
    }
    if (ct_clamp->get_current_sensor() != nullptr && measurement.has_current) {
      ct_clamp->get_current_sensor()->publish_state(measurement.current);
    }

    if (ct_clamp->get_apparent_power_sensor() != nullptr && measurement.has_apparent_power) {
      const bool above_threshold = measurement.apparent_power >= this->power_apparent_min_;
      ct_clamp->get_apparent_power_sensor()->publish_state(above_threshold ? measurement.apparent_power : 0.0f);
    }
    if (ct_clamp->get_power_factor_sensor() != nullptr && measurement.has_power_factor) {
      ct_clamp->get_power_factor_sensor()->publish_state(measurement.power_factor);
    }
    this->update_phase_detection_(frame, ct_clamp);
  }

  for (auto *group : this->metering_groups_) {
    if (group->get_power_outputs().empty()) {
      continue;
    }

    float group_power = 0.0f;
    if (this->calculate_group_power_(frame, group, &group_power)) {
      publish_power_outputs_(group->get_power_outputs(), group_power);
    }
  }
}

void EmporiaVueComponent::refresh_metering_() {
  if (this->backup_active_ || this->install_active_) {
    return;
  }
  if (this->runtime_mode_ != RuntimeMode::I2C) {
    return;
  }
  if (!this->firmware_mode_matches_runtime_()) {
    this->publish_firmware_mode_mismatch_();
    return;
  }

  MeteringFrame frame{};
  const I2CMeteringReadResult result = this->read_i2c_metering_frame_(&frame);
  if (result != I2CMeteringReadResult::VALID_FRAME) {
    return;
  }

  this->submit_metering_frame_(frame);
}

void EmporiaVueComponent::start_metering_() {
  if (this->metering_started_) {
    return;
  }
  if (this->metering_interval_ms_ == 0 || this->runtime_mode_ != RuntimeMode::I2C) {
    return;
  }
  if (!this->firmware_mode_matches_runtime_()) {
    this->start_firmware_mode_mismatch_log_();
    return;
  }

  this->stop_firmware_mode_mismatch_log_();
  this->metering_started_ = true;
  this->set_interval("samd_i2c_metering", this->metering_interval_ms_, [this]() { this->refresh_metering_(); });
}

void EmporiaVueComponent::stop_metering_() {
  if (!this->metering_started_) {
    return;
  }
  this->cancel_interval("samd_i2c_metering");
  this->metering_started_ = false;
}

}  // namespace emporiavue
}  // namespace esphome
