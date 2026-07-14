#include "emporiavue.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <string>

namespace esphome {
namespace emporiavue {

void MeteringDemandTracker::setup() {
  if (!this->enabled()) {
    return;
  }
  const size_t buckets = std::max<size_t>(1, this->interval_ms_ / BUCKET_DURATION_MS);
  this->buckets_.assign(buckets, 0.0f);
  this->reset_window_(false);

  if (this->maximum_sensor_ != nullptr && this->restore_) {
    this->pref_ = global_preferences->make_preference<RestoreState>(this->maximum_sensor_->get_object_id_hash());
    this->pref_initialized_ = true;
    this->restored_state_valid_ = this->pref_.load(&this->restored_state_);
  }
}

void MeteringDemandTracker::loop() {
  if (this->maximum_sensor_ != nullptr) {
    this->check_day_(millis());
  }
}

void MeteringDemandTracker::reset_window_(bool publish_unavailable) {
  std::fill(this->buckets_.begin(), this->buckets_.end(), 0.0f);
  this->bucket_index_ = 0;
  this->bucket_count_ = 0;
  this->window_sum_ = 0.0;
  this->current_bucket_integral_ = 0.0;
  this->current_bucket_elapsed_ms_ = 0;
  this->sample_initialized_ = false;
  if (publish_unavailable && this->demand_sensor_ != nullptr) {
    this->demand_sensor_->publish_state(NAN);
  }
}

uint32_t MeteringDemandTracker::day_key_(const ESPTime &now) {
  return static_cast<uint32_t>(now.year) * 1000U + now.day_of_year;
}

void MeteringDemandTracker::publish_and_save_maximum_(float value) {
  this->daily_maximum_ = value;
  this->daily_maximum_valid_ = !std::isnan(value);
  if (this->maximum_sensor_ != nullptr) {
    this->maximum_sensor_->publish_state(value);
  }
  if (this->pref_initialized_) {
    const RestoreState state{this->current_day_key_, value};
    this->pref_.save(&state);
  }
}

void MeteringDemandTracker::reset_daily_maximum_(uint32_t day_key, uint32_t now_ms) {
  this->current_day_key_ = day_key;
  this->daily_window_start_ms_ = now_ms;
  this->publish_and_save_maximum_(NAN);
}

void MeteringDemandTracker::check_day_(uint32_t now_ms) {
  if (this->maximum_sensor_ == nullptr || this->time_ == nullptr) {
    return;
  }
  const ESPTime now = this->time_->now();
  if (!now.is_valid()) {
    return;
  }
  const uint32_t day_key = day_key_(now);
  if (this->current_day_key_ == 0) {
    if (this->restored_state_valid_ && this->restored_state_.day_key == day_key) {
      this->current_day_key_ = day_key;
      this->daily_maximum_ = this->restored_state_.maximum;
      this->daily_maximum_valid_ = !std::isnan(this->daily_maximum_);
      this->daily_window_start_ms_ =
          this->daily_maximum_valid_ ? now_ms - this->interval_ms_ : now_ms;
      this->maximum_sensor_->publish_state(this->daily_maximum_);
    } else {
      this->reset_daily_maximum_(day_key, now_ms);
    }
    return;
  }
  if (day_key != this->current_day_key_) {
    this->reset_daily_maximum_(day_key, now_ms);
  }
}

void MeteringDemandTracker::finish_bucket_(uint32_t now_ms) {
  if (this->buckets_.empty()) {
    return;
  }
  const float average = static_cast<float>(this->current_bucket_integral_ / BUCKET_DURATION_MS);
  if (this->bucket_count_ == this->buckets_.size()) {
    this->window_sum_ -= this->buckets_[this->bucket_index_];
  } else {
    this->bucket_count_++;
  }
  this->buckets_[this->bucket_index_] = average;
  this->window_sum_ += average;
  this->bucket_index_ = (this->bucket_index_ + 1) % this->buckets_.size();
  this->current_bucket_integral_ = 0.0;
  this->current_bucket_elapsed_ms_ = 0;

  if (this->bucket_count_ != this->buckets_.size()) {
    return;
  }
  const float demand = static_cast<float>(this->window_sum_ / this->buckets_.size());
  if (this->demand_sensor_ != nullptr) {
    this->demand_sensor_->publish_state(demand);
  }
  if (this->maximum_sensor_ == nullptr) {
    return;
  }
  this->check_day_(now_ms);
  if (this->current_day_key_ == 0 || (now_ms - this->daily_window_start_ms_) < this->interval_ms_) {
    return;
  }
  if (!this->daily_maximum_valid_ || demand > this->daily_maximum_) {
    this->publish_and_save_maximum_(demand);
  }
}

void MeteringDemandTracker::add_sample(float value, uint32_t now_ms) {
  if (!this->enabled()) {
    return;
  }
  if (std::isnan(value)) {
    this->reset_window_(true);
    return;
  }
  if (!this->sample_initialized_) {
    this->last_sample_ms_ = now_ms;
    this->last_value_ = value;
    this->sample_initialized_ = true;
    return;
  }

  uint32_t elapsed_ms = now_ms - this->last_sample_ms_;
  this->last_sample_ms_ = now_ms;
  if (elapsed_ms > MAX_SAMPLE_GAP_MS) {
    this->reset_window_(true);
    this->last_sample_ms_ = now_ms;
    this->last_value_ = value;
    this->sample_initialized_ = true;
    return;
  }

  while (elapsed_ms > 0) {
    const uint32_t remaining_ms = BUCKET_DURATION_MS - this->current_bucket_elapsed_ms_;
    const uint32_t chunk_ms = std::min(elapsed_ms, remaining_ms);
    this->current_bucket_integral_ += static_cast<double>(this->last_value_) * chunk_ms;
    this->current_bucket_elapsed_ms_ += chunk_ms;
    elapsed_ms -= chunk_ms;
    if (this->current_bucket_elapsed_ms_ == BUCKET_DURATION_MS) {
      this->finish_bucket_(now_ms - elapsed_ms);
    }
  }
  this->last_value_ = value;
}

void MeteringPeakTracker::finish_window_(uint32_t now_ms) {
  if (this->current_peak_sensor_ != nullptr) {
    this->current_peak_sensor_->publish_state(this->current_peak_valid_ ? this->maximum_current_peak_ : NAN);
  }
  if (this->current_crest_factor_sensor_ != nullptr) {
    this->current_crest_factor_sensor_->publish_state(
        this->current_crest_factor_valid_ ? this->maximum_current_crest_factor_ : NAN);
  }
  this->window_start_ms_ = now_ms;
  this->maximum_current_peak_ = 0.0f;
  this->maximum_current_crest_factor_ = 0.0f;
  this->window_initialized_ = false;
  this->current_peak_valid_ = false;
  this->current_crest_factor_valid_ = false;
}

void MeteringPeakTracker::loop(uint32_t now_ms) {
  if (this->window_initialized_ && (now_ms - this->window_start_ms_) >= this->interval_ms_) {
    this->finish_window_(now_ms);
  }
}

void MeteringPeakTracker::add_sample(float current_peak, float current_crest_factor, uint32_t now_ms) {
  if (!this->enabled()) {
    return;
  }
  if (this->window_initialized_ && (now_ms - this->window_start_ms_) >= this->interval_ms_) {
    this->finish_window_(now_ms);
  }
  if (!this->window_initialized_) {
    this->window_start_ms_ = now_ms;
    this->window_initialized_ = true;
  }
  if (!std::isnan(current_peak)) {
    this->maximum_current_peak_ =
        this->current_peak_valid_ ? std::max(this->maximum_current_peak_, current_peak) : current_peak;
    this->current_peak_valid_ = true;
  }
  if (!std::isnan(current_crest_factor)) {
    this->maximum_current_crest_factor_ = this->current_crest_factor_valid_
                                              ? std::max(this->maximum_current_crest_factor_, current_crest_factor)
                                              : current_crest_factor;
    this->current_crest_factor_valid_ = true;
  }
}

void EmporiaVueComponent::submit_metering_frame_(const MeteringFrame &frame) {
  if (!frame.valid) {
    return;
  }

  if (this->last_metering_sequence_valid_ && frame.transport == MeteringTransport::I2C &&
      this->last_metering_transport_ == MeteringTransport::I2C) {
    const uint8_t sequence_delta = static_cast<uint8_t>(frame.sequence - this->last_metering_sequence_);
    if (sequence_delta > 1) {
      this->i2c_missing_readings_window_ += static_cast<uint8_t>(sequence_delta - 1);
    }
  }

  this->last_metering_sequence_ = frame.sequence;
  this->last_metering_transport_ = frame.transport;
  this->last_metering_sequence_valid_ = true;

  const char *transport = frame.transport == MeteringTransport::SPI ? "SPI" :
                          frame.transport == MeteringTransport::I2C ? "I2C" : "unknown";
  ESP_LOGV(TAG, "SAMD09 %s metering frame: seq=%" PRIu32 ", flags=0x%02x", transport, frame.sequence,
           frame.quality_flags);
  this->publish_metering_frame_(frame);
}

bool EmporiaVueComponent::calculate_ct_fundamental_phasors_(
    const MeteringFrame &frame, const MeteringCTClampConfig *ct_clamp, float *voltage_i, float *voltage_q,
    float *current_i, float *current_q, bool apply_phase_correction) const {
  if (ct_clamp == nullptr || voltage_i == nullptr || voltage_q == nullptr || current_i == nullptr ||
      current_q == nullptr || frame.transport != MeteringTransport::SPI) {
    return false;
  }

  const uint8_t port = ct_clamp->get_input_port();
  const MeteringPhaseConfig *phase_a = ct_clamp->get_phase();
  if (port >= 19 || phase_a == nullptr || phase_a->get_input_wire() >= 3 ||
      !frame.clamps[port].current_fundamental_valid) {
    return false;
  }

  auto voltage_phasor = [&frame](const MeteringPhaseConfig *phase, float *i, float *q) -> bool {
    if (phase == nullptr || i == nullptr || q == nullptr || phase->get_input_wire() >= 3) {
      return false;
    }
    const auto &value = frame.phases[phase->get_input_wire()];
    if (!value.voltage_fundamental_valid) {
      return false;
    }
    *i = value.voltage_fundamental_i_raw * phase->get_calibration();
    *q = value.voltage_fundamental_q_raw * phase->get_calibration();
    return true;
  };

  if (!voltage_phasor(phase_a, voltage_i, voltage_q)) {
    return false;
  }
  if (ct_clamp->is_line_pair()) {
    float voltage_b_i = 0.0f;
    float voltage_b_q = 0.0f;
    if (!voltage_phasor(ct_clamp->get_line_pair_phase_b(), &voltage_b_i, &voltage_b_q)) {
      return false;
    }
    *voltage_i -= voltage_b_i;
    *voltage_q -= voltage_b_q;
  }

  const float current_scalar = port < 3 ? (775.0f / 42624.0f) : (775.0f / 170496.0f);
  *current_i = frame.clamps[port].current_fundamental_i_raw * current_scalar * ct_clamp->get_current_gain();
  *current_q = frame.clamps[port].current_fundamental_q_raw * current_scalar * ct_clamp->get_current_gain();
  if (apply_phase_correction && ct_clamp->get_current_phase_correction() != 0.0f) {
    constexpr float degrees_to_radians = 3.14159265358979323846f / 180.0f;
    const float angle = ct_clamp->get_current_phase_correction() * degrees_to_radians;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float corrected_i = *current_i * cosine - *current_q * sine;
    const float corrected_q = *current_i * sine + *current_q * cosine;
    *current_i = corrected_i;
    *current_q = corrected_q;
  }
  return true;
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
  const uint8_t phase_a_input = phase->get_input_wire();
  const auto &clamp = frame.clamps[port];
  if ((clamp.power_phase_valid_mask & (1U << phase_a_input)) == 0) {
    return false;
  }

  const float correction_factor = port < 3 ? 5.5f : 22.0f;
  float corrected_power = 0.0f;
  if (ct_clamp->is_line_pair()) {
    const MeteringPhaseConfig *phase_b = ct_clamp->get_line_pair_phase_b();
    if (phase_b == nullptr || phase_b->get_input_wire() >= 3) {
      return false;
    }
    const uint8_t phase_b_input = phase_b->get_input_wire();
    if ((clamp.power_phase_valid_mask & (1U << phase_b_input)) == 0) {
      return false;
    }

    const int32_t raw_power_a = clamp.power_raw_by_phase[phase_a_input];
    const int32_t raw_power_b = clamp.power_raw_by_phase[phase_b_input];
    const float power_a = raw_power_a * phase->get_calibration() / correction_factor;
    const float power_b = raw_power_b * phase_b->get_calibration() / correction_factor;
    corrected_power = (power_a - power_b) * ct_clamp->get_current_gain();
  } else {
    const int32_t raw_power = clamp.power_raw_by_phase[phase_a_input];
    corrected_power = raw_power * phase->get_calibration() / correction_factor * ct_clamp->get_current_gain();
  }

  if (frame.transport == MeteringTransport::SPI && ct_clamp->get_current_phase_correction() != 0.0f) {
    float voltage_i = 0.0f;
    float voltage_q = 0.0f;
    float measured_current_i = 0.0f;
    float measured_current_q = 0.0f;
    float corrected_current_i = 0.0f;
    float corrected_current_q = 0.0f;
    if (this->calculate_ct_fundamental_phasors_(frame, ct_clamp, &voltage_i, &voltage_q, &measured_current_i,
                                                &measured_current_q, false) &&
        this->calculate_ct_fundamental_phasors_(frame, ct_clamp, &voltage_i, &voltage_q, &corrected_current_i,
                                                &corrected_current_q, true)) {
      const float measured_fundamental_power =
          voltage_i * measured_current_i + voltage_q * measured_current_q;
      const float corrected_fundamental_power =
          voltage_i * corrected_current_i + voltage_q * corrected_current_q;
      corrected_power += corrected_fundamental_power - measured_fundamental_power;
    }
  }

  *power = ct_clamp->apply_power_filters(corrected_power);
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
  if (frame.voltage_statistics_valid && input_a != input_b) {
    uint8_t pair = 2;
    if (input_a == 0 || input_b == 0) {
      pair = input_a == 0 ? static_cast<uint8_t>(input_b - 1) : static_cast<uint8_t>(input_a - 1);
    }
    const float calibration_a = line_a->get_calibration();
    const float calibration_b = line_b->get_calibration();
    const float voltage_sq = frame.voltage_square_raw[input_a] * calibration_a * calibration_a +
                             frame.voltage_square_raw[input_b] * calibration_b * calibration_b -
                             2.0f * frame.voltage_product_raw[pair] * calibration_a * calibration_b;
    *voltage = std::sqrt(std::max(0.0f, voltage_sq));
    return true;
  }

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
  *current = frame.clamps[port].current_raw * scalar * ct_clamp->get_current_gain();
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
    if (measurement->apparent_power >= this->minimum_apparent_power_ && measurement->apparent_power > 0.0f &&
        measurement->has_power) {
      measurement->power_factor =
          std::max(0.0f, std::min(1.0f, std::fabs(measurement->power) / measurement->apparent_power));
    }
  }
  return measurement->has_power || measurement->has_voltage || measurement->has_current ||
         measurement->has_apparent_power;
}

bool EmporiaVueComponent::calculate_ct_fundamental_analysis_(const MeteringFrame &frame,
                                                             const MeteringCTClampConfig *ct_clamp,
                                                             MeteringCTMeasurement *measurement) const {
  if (ct_clamp == nullptr || measurement == nullptr || frame.transport != MeteringTransport::SPI) {
    return false;
  }

  float voltage_i = 0.0f;
  float voltage_q = 0.0f;
  float current_i = 0.0f;
  float current_q = 0.0f;
  if (!this->calculate_ct_fundamental_phasors_(frame, ct_clamp, &voltage_i, &voltage_q, &current_i, &current_q,
                                               true)) {
    return false;
  }
  const float fundamental_current = std::hypot(current_i, current_q);
  measurement->has_fundamental_analysis = true;
  measurement->fundamental_current = fundamental_current;

  if (fundamental_current < this->minimum_fundamental_current_) {
    measurement->fundamental_current = 0.0f;
    measurement->fundamental_reactive_power = 0.0f;
    return true;
  }

  const bool reactive_power_required = ct_clamp->get_fundamental_reactive_power_sensor() != nullptr;
  const bool power_factor_required = ct_clamp->get_fundamental_power_factor_sensor() != nullptr;
  const bool displacement_angle_required = ct_clamp->get_displacement_angle_sensor() != nullptr;
  const bool current_thd_required = ct_clamp->get_current_thd_sensor() != nullptr;

  float active_power = 0.0f;
  float reactive_power = 0.0f;
  if (power_factor_required || displacement_angle_required) {
    active_power = voltage_i * current_i + voltage_q * current_q;
  }
  if (reactive_power_required || displacement_angle_required) {
    reactive_power = voltage_i * current_q - voltage_q * current_i;
  }
  if (reactive_power_required) {
    measurement->fundamental_reactive_power = reactive_power;
  }
  if (power_factor_required || displacement_angle_required) {
    const float fundamental_voltage = std::hypot(voltage_i, voltage_q);
    const float apparent_power = fundamental_voltage * fundamental_current;
    if (apparent_power > 0.0f) {
      if (power_factor_required) {
        measurement->fundamental_power_factor =
            std::max(0.0f, std::min(1.0f, std::fabs(active_power) / apparent_power));
      }
      if (displacement_angle_required) {
        measurement->displacement_angle =
            std::atan2(reactive_power, active_power) * 180.0f / 3.14159265358979323846f;
      }
    }
  }

  if (current_thd_required && measurement->has_current && fundamental_current > 0.0f) {
    const float consistency_tolerance = std::max(0.001f, measurement->current * 0.01f);
    if (fundamental_current <= measurement->current + consistency_tolerance) {
      const float harmonic_current_sq =
          std::max(0.0f, measurement->current * measurement->current - fundamental_current * fundamental_current);
      measurement->current_thd = std::sqrt(harmonic_current_sq) * 100.0f / fundamental_current;
    }
  }
  return true;
}

float EmporiaVueComponent::apply_power_direction_(float power, uint8_t direction) {
  if (std::isnan(power)) {
    return power;
  }
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

bool EmporiaVueComponent::calculate_group_power_(const MeteringFrame &frame, MeteringGroupConfig *group,
                                                 float *group_power, uint32_t generation) const {
  if (group == nullptr || group_power == nullptr) {
    return false;
  }

  auto &group_cache = group->get_power_cache();
  switch (group_cache.get_state(generation)) {
    case MeteringPowerCache::State::VALID:
      *group_power = group_cache.get_value();
      return true;
    case MeteringPowerCache::State::VISITING:
    case MeteringPowerCache::State::INVALID:
      return false;
    case MeteringPowerCache::State::EMPTY:
      break;
  }
  group_cache.mark_visiting(generation);

  float sum = 0.0f;
  bool has_power = false;
  for (const auto &term : group->get_terms()) {
    if (term.ct_clamp != nullptr) {
      float power = 0.0f;
      auto &ct_cache = term.ct_clamp->get_power_cache();
      switch (ct_cache.get_state(generation)) {
        case MeteringPowerCache::State::VALID:
          power = ct_cache.get_value();
          break;
        case MeteringPowerCache::State::INVALID:
        case MeteringPowerCache::State::VISITING:
          group_cache.mark_invalid(generation);
          return false;
        case MeteringPowerCache::State::EMPTY:
          if (!this->calculate_ct_power_(frame, term.ct_clamp, &power)) {
            ct_cache.mark_invalid(generation);
            group_cache.mark_invalid(generation);
            return false;
          }
          ct_cache.store(generation, power);
          break;
      }
      sum += term.sign * power;
      has_power = true;
      continue;
    }

    if (term.group != nullptr) {
      float power = 0.0f;
      if (!this->calculate_group_power_(frame, term.group, &power, generation)) {
        group_cache.mark_invalid(generation);
        return false;
      }
      sum += term.sign * power;
      has_power = true;
    }
  }
  if (!has_power) {
    group_cache.mark_invalid(generation);
    return false;
  }

  *group_power = group->apply_power_filters(sum);
  group_cache.store(generation, *group_power);
  return true;
}

void EmporiaVueComponent::update_phase_detection_(const MeteringFrame &frame, MeteringCTClampConfig *ct_clamp) {
  if (ct_clamp == nullptr ||
      (ct_clamp->get_phase_detection_sensor() == nullptr && !ct_clamp->is_auto_line_detection_active())) {
    return;
  }

  const char *detection_name = ct_clamp->get_phase_detection_name().empty()
                                   ? "Automatic line detection"
                                   : ct_clamp->get_phase_detection_name().c_str();

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
    if (input >= 3 || (frame.clamps[port].power_phase_valid_mask & (1U << input)) == 0) {
      continue;
    }
    const int32_t raw_power = frame.clamps[port].power_raw_by_phase[input];
    const float power = raw_power * candidate.phase->get_calibration() / correction_factor *
                        ct_clamp->get_current_gain();
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
    if (ct_clamp->get_phase_detection_sensor() != nullptr) {
      ct_clamp->get_phase_detection_sensor()->publish_state(state);
    }
    ESP_LOGV(TAG, "%s: low load window=%" PRIu32 "ms power_min=%.1fW",
             detection_name, now - window_start, ct_clamp->get_phase_detection_power_min());
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
  bool auto_detection_complete = false;
  if (ratio < this->phase_detection_confidence_ratio_) {
    ct_clamp->reset_phase_detection_stability();
    state = str_sprintf("ambiguous L%u/L%u", static_cast<unsigned>(best_line), static_cast<unsigned>(second_line));
  } else {
    const uint8_t stable_windows = ct_clamp->update_phase_detection_candidate(best_line);
    if (stable_windows >= 3) {
      state = str_sprintf("L%u", static_cast<unsigned>(best_line));
      auto_detection_complete = ct_clamp->is_auto_line_detection_active();
    } else {
      state = str_sprintf("L%u weak", static_cast<unsigned>(best_line));
    }
  }

  if (ct_clamp->get_phase_detection_sensor() != nullptr) {
    ct_clamp->get_phase_detection_sensor()->publish_state(state);
  }

  const float divisor = samples == 0 ? 1.0f : static_cast<float>(samples);
  ESP_LOGV(TAG,
           "%s: state=%s window=%" PRIu32 "ms samples=%" PRIu32
           " scores L1=%.1fW L2=%.1fW L3=%.1fW confidence=%.0f%% power_min=%.1fW",
           detection_name, state.c_str(), now - window_start, samples,
           scores[0] / divisor, scores[1] / divisor, scores[2] / divisor,
           confidence, ct_clamp->get_phase_detection_power_min());

  ct_clamp->reset_phase_detection();
  ct_clamp->set_phase_detection_window_start_ms(now);
  if (auto_detection_complete) {
    ESP_LOGI(TAG, "%s: selected L%u and stored the assignment", detection_name,
             static_cast<unsigned>(best_line));
    ct_clamp->complete_auto_line_detection(best_line);
  }
}

void EmporiaVueComponent::publish_metering_frame_(const MeteringFrame &frame) {
  if (!frame.valid) {
    return;
  }
  const uint32_t demand_now_ms = millis();
  if (++this->metering_calculation_generation_ == 0) {
    this->metering_calculation_generation_ = 1;
  }
  const uint32_t calculation_generation = this->metering_calculation_generation_;

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
    if (phase->get_voltage_thd_sensor() != nullptr) {
      phase->get_voltage_thd_sensor()->publish_state(frame.phases[input].voltage_thd_valid
                                                         ? frame.phases[input].voltage_thd_percent
                                                         : std::numeric_limits<float>::quiet_NaN());
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
    this->update_phase_detection_(frame, ct_clamp);
    const MeteringPhaseConfig *phase = ct_clamp->get_phase();
    if (port >= 19) {
      continue;
    }

    MeteringCTMeasurement measurement{};
    this->calculate_ct_measurement_(frame, ct_clamp, &measurement);
    if (measurement.has_power) {
      ct_clamp->get_power_cache().store(calculation_generation, measurement.power);
    } else {
      ct_clamp->get_power_cache().mark_invalid(calculation_generation);
    }
    const bool has_fundamental_analysis = ct_clamp->has_fundamental_analysis() &&
                                          this->calculate_ct_fundamental_analysis_(frame, ct_clamp, &measurement);
    const float unavailable = std::numeric_limits<float>::quiet_NaN();
    if (measurement.has_power) {
      ct_clamp->add_power_demand_sample(measurement.power, demand_now_ms);
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
    } else if (phase == nullptr) {
      publish_power_outputs_(ct_clamp->get_power_outputs(), unavailable);
    }
    if (ct_clamp->get_current_sensor() != nullptr && measurement.has_current) {
      ct_clamp->get_current_sensor()->publish_state(measurement.current);
    }
    if (measurement.has_current) {
      ct_clamp->add_current_demand_sample(measurement.current, demand_now_ms);
    }
    if (ct_clamp->has_peak_analysis() && frame.transport == MeteringTransport::SPI && measurement.has_current &&
        frame.clamps[port].current_peak_valid) {
      const float current_scalar = port < 3 ? (775.0f / 42624.0f) : (775.0f / 170496.0f);
      float current_peak = frame.clamps[port].current_peak_raw * current_scalar * ct_clamp->get_current_gain();
      float current_crest_factor = unavailable;
      if (measurement.current >= this->minimum_fundamental_current_ && measurement.current > 0.0f) {
        current_crest_factor = current_peak / measurement.current;
      } else {
        current_peak = 0.0f;
      }
      ct_clamp->add_peak_sample(current_peak, current_crest_factor, demand_now_ms);
    }

    if (ct_clamp->get_apparent_power_sensor() != nullptr && measurement.has_apparent_power) {
      const bool above_threshold = measurement.apparent_power >= this->minimum_apparent_power_;
      ct_clamp->get_apparent_power_sensor()->publish_state(above_threshold ? measurement.apparent_power : 0.0f);
    } else if (ct_clamp->get_apparent_power_sensor() != nullptr && phase == nullptr) {
      ct_clamp->get_apparent_power_sensor()->publish_state(unavailable);
    }
    if (ct_clamp->get_power_factor_sensor() != nullptr && measurement.has_power_factor) {
      ct_clamp->get_power_factor_sensor()->publish_state(measurement.power_factor);
    } else if (ct_clamp->get_power_factor_sensor() != nullptr && phase == nullptr) {
      ct_clamp->get_power_factor_sensor()->publish_state(unavailable);
    }
    if (ct_clamp->get_fundamental_current_sensor() != nullptr) {
      ct_clamp->get_fundamental_current_sensor()->publish_state(
          has_fundamental_analysis ? measurement.fundamental_current : unavailable);
    }
    if (ct_clamp->get_fundamental_reactive_power_sensor() != nullptr) {
      ct_clamp->get_fundamental_reactive_power_sensor()->publish_state(
          has_fundamental_analysis ? measurement.fundamental_reactive_power : unavailable);
    }
    if (ct_clamp->get_fundamental_power_factor_sensor() != nullptr) {
      ct_clamp->get_fundamental_power_factor_sensor()->publish_state(
          has_fundamental_analysis ? measurement.fundamental_power_factor : unavailable);
    }
    if (ct_clamp->get_displacement_angle_sensor() != nullptr) {
      ct_clamp->get_displacement_angle_sensor()->publish_state(
          has_fundamental_analysis ? measurement.displacement_angle : unavailable);
    }
    if (ct_clamp->get_current_thd_sensor() != nullptr) {
      ct_clamp->get_current_thd_sensor()->publish_state(
          has_fundamental_analysis ? measurement.current_thd : unavailable);
    }
  }

  for (auto *group : this->metering_groups_) {
    if (group->get_power_outputs().empty() && !group->has_power_demand()) {
      continue;
    }

    float group_power = 0.0f;
    if (this->calculate_group_power_(frame, group, &group_power, calculation_generation)) {
      group->add_power_demand_sample(group_power, demand_now_ms);
      publish_power_outputs_(group->get_power_outputs(), group_power);
    } else {
      publish_power_outputs_(group->get_power_outputs(), std::numeric_limits<float>::quiet_NaN());
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
  if (result == I2CMeteringReadResult::VALID_FRAME) {
    this->submit_metering_frame_(frame);
  }
  this->log_i2c_metering_status_();
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
  this->reset_i2c_metering_status_();
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
