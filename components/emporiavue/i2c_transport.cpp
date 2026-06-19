#include "emporiavue.h"

#include "esphome/core/log.h"

#include <cinttypes>
#include <cstddef>

namespace esphome {
namespace emporiavue {

static uint8_t metering_crc8_update(uint8_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0; bit < 8; bit++) {
    crc = (crc & 0x80) != 0 ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
  }
  return crc;
}

EmporiaVueComponent::ManagedI2CDiagnosticResult EmporiaVueComponent::query_managed_i2c_diagnostic_(
    ManagedI2CDiagnostic *diagnostic) {
#ifdef USE_I2C
  ManagedI2CDiagnostic candidate{};
  const uint8_t command = MANAGED_I2C_DIAGNOSTIC_COMMAND;
  const i2c::ErrorCode error =
      this->write_read(&command, 1, reinterpret_cast<uint8_t *>(&candidate), sizeof(candidate));
  if (error != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "SAMD09 I2C diagnostic query failed: i2c error %u", static_cast<unsigned>(error));
    return ManagedI2CDiagnosticResult::I2C_ERROR;
  }
  if (!this->validate_managed_i2c_diagnostic_(candidate)) {
    ESP_LOGD(TAG, "SAMD09 I2C diagnostic query returned no valid response");
    return ManagedI2CDiagnosticResult::INVALID_RESPONSE;
  }

  *diagnostic = candidate;
  ESP_LOGD(TAG,
           "SAMD09 I2C diagnostic: seq=%" PRIu32 ", samples=%" PRIu32 ", built=%" PRIu32
           ", read=%" PRIu32 ", dma_errors=%" PRIu32 ", overruns=%" PRIu32,
           candidate.diagnostic_sequence, candidate.sample_blocks, candidate.packets_built, candidate.packets_read,
           candidate.dma_transfer_errors, candidate.packet_overruns);
  return ManagedI2CDiagnosticResult::VALID_RESPONSE;
#else
  (void) diagnostic;
  return ManagedI2CDiagnosticResult::I2C_ERROR;
#endif
}

bool EmporiaVueComponent::validate_managed_i2c_diagnostic_(const ManagedI2CDiagnostic &diagnostic) const {
  const uint32_t expected_crc =
      crc32_(reinterpret_cast<const uint8_t *>(&diagnostic), offsetof(ManagedI2CDiagnostic, crc32));
  if (expected_crc != diagnostic.crc32) {
    return false;
  }
  if (diagnostic.hardware_id != 2 && diagnostic.hardware_id != 3) {
    return false;
  }
  if (diagnostic.firmware_version == 0) {
    return false;
  }
  return diagnostic.i2c_frame_length == STOCK_I2C_FRAME_SIZE;
}

void EmporiaVueComponent::publish_firmware_info_from_diagnostic_(const ManagedI2CDiagnostic &diagnostic) {
  FirmwareInfo info{};
  info.kind = FirmwareKind::MANAGED;
  info.hardware_id = diagnostic.hardware_id;
  info.mode_id = MANAGED_MODE_I2C;
  info.version = diagnostic.firmware_version;
  info.i2c_frame_length = diagnostic.i2c_frame_length;
  info.source = FirmwareDetectionSource::I2C;
  const bool changed = !this->detected_firmware_info_valid_ ||
                       this->detected_firmware_info_.kind != FirmwareKind::MANAGED ||
                       this->detected_firmware_info_.hardware_id != info.hardware_id ||
                       this->detected_firmware_info_.mode_id != info.mode_id ||
                       this->detected_firmware_info_.version != info.version;
  this->detected_firmware_info_ = info;
  this->detected_firmware_info_valid_ = true;
  if (changed) {
    this->publish_firmware_version_(info);
  }
}

void EmporiaVueComponent::refresh_i2c_diagnostics_() {
  if (this->backup_active_ || this->install_active_) {
    return;
  }
  if (this->runtime_mode_ != RuntimeMode::I2C) {
    return;
  }
  if (!this->detected_firmware_info_valid_ || this->detected_firmware_info_.kind != FirmwareKind::MANAGED) {
    return;
  }
  if (!this->firmware_mode_matches_runtime_()) {
    this->publish_firmware_mode_mismatch_();
    return;
  }

  ManagedI2CDiagnostic diagnostic{};
  const ManagedI2CDiagnosticResult result = this->query_managed_i2c_diagnostic_(&diagnostic);
  if (result == ManagedI2CDiagnosticResult::VALID_RESPONSE) {
    this->publish_firmware_info_from_diagnostic_(diagnostic);
    this->publish_i2c_diagnostics_(diagnostic);
  }
}

void EmporiaVueComponent::publish_i2c_diagnostics_(const ManagedI2CDiagnostic &diagnostic) {
  if (this->diag_frame_errors_sensor_ != nullptr) {
    this->diag_frame_errors_sensor_->publish_state(
        static_cast<float>(diagnostic.i2c_partial_reads + diagnostic.i2c_oversize_reads));
  }
  if (this->diag_transfer_errors_sensor_ != nullptr) {
    this->diag_transfer_errors_sensor_->publish_state(static_cast<float>(diagnostic.dma_transfer_errors));
  }
  if (this->diag_frame_overruns_sensor_ != nullptr) {
    this->diag_frame_overruns_sensor_->publish_state(static_cast<float>(diagnostic.packet_overruns));
  }
  if (this->diag_recoveries_sensor_ != nullptr) {
    this->diag_recoveries_sensor_->publish_state(0.0f);
  }
  if (this->diag_last_frame_samples_sensor_ != nullptr) {
    this->diag_last_frame_samples_sensor_->publish_state(static_cast<float>(diagnostic.last_sample_count));
  }
  if (this->diag_sample_rate_sensor_ != nullptr) {
    this->diag_sample_rate_sensor_->publish_state(
        this->hardware_id_ == 3 ? VUE3_STOCK_CYCLE_TIMEBASE_HZ : VUE2_STOCK_CYCLE_TIMEBASE_HZ);
  }
}

EmporiaVueComponent::I2CMeteringReadResult EmporiaVueComponent::read_i2c_metering_frame_(MeteringFrame *frame) {
#ifdef USE_I2C
  static_assert(sizeof(I2CMeteringPacket) == STOCK_I2C_FRAME_SIZE, "I2C metering packet size changed");

  I2CMeteringPacket packet{};
  const i2c::ErrorCode error = this->read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
  if (error != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "SAMD09 metering I2C read failed: i2c error %u", static_cast<unsigned>(error));
    return I2CMeteringReadResult::ERROR;
  }

  if (this->hardware_id_ == 3) {
    if (packet.is_unread != 3) {
      return I2CMeteringReadResult::STALE_FRAME;
    }
  } else if (packet.is_unread == 0) {
    return I2CMeteringReadResult::STALE_FRAME;
  }

  const uint8_t checksum = this->calculate_i2c_metering_checksum_(packet);
  if (checksum != packet.checksum) {
    return I2CMeteringReadResult::STALE_FRAME;
  }

  return this->decode_i2c_metering_packet_(packet, frame) ? I2CMeteringReadResult::VALID_FRAME
                                                          : I2CMeteringReadResult::STALE_FRAME;
#else
  (void) frame;
  return I2CMeteringReadResult::ERROR;
#endif
}

uint8_t EmporiaVueComponent::calculate_i2c_metering_checksum_(const I2CMeteringPacket &packet) const {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&packet);
  uint8_t crc = metering_crc8_update(0xDE, bytes[0]);
  for (uint16_t index = 2; index < STOCK_I2C_FRAME_SIZE; index++) {
    crc = metering_crc8_update(crc, bytes[index]);
  }
  return crc;
}

bool EmporiaVueComponent::decode_i2c_metering_packet_(const I2CMeteringPacket &packet, MeteringFrame *frame) const {
  if (this->hardware_id_ != 3 && packet.end != 0) {
    ESP_LOGD(TAG, "SAMD09 metering I2C packet malformed: end=0x%04x", packet.end);
    return false;
  }

  MeteringFrame candidate{};
  candidate.schema_version = 1;
  candidate.transport = MeteringTransport::I2C;
  candidate.sequence = packet.sequence_num;
  candidate.timestamp_ms = millis();
  candidate.valid = true;

  for (uint8_t phase = 0; phase < 3; phase++) {
    candidate.phases[phase].voltage_raw = packet.voltage[phase];
    candidate.phases[phase].cycle_count_raw = packet.cycle_count[phase];
  }
  const float stock_timebase =
      this->hardware_id_ == 3 ? VUE3_STOCK_CYCLE_TIMEBASE_HZ : VUE2_STOCK_CYCLE_TIMEBASE_HZ;
  const uint16_t stock_period = candidate.phases[0].cycle_count_raw;
  if (stock_period != 0) {
    const float frequency = stock_timebase / static_cast<float>(stock_period);
    for (uint8_t phase = 0; phase < 3; phase++) {
      candidate.phases[phase].frequency_hz = frequency;
      candidate.phases[phase].phase_angle_degrees =
          phase == 0 ? 0.0f : candidate.phases[phase].cycle_count_raw * 360.0f / static_cast<float>(stock_period);
    }
  }
  for (uint8_t clamp = 0; clamp < 19; clamp++) {
    candidate.clamps[clamp].current_raw = packet.current[clamp];
    for (uint8_t phase = 0; phase < 3; phase++) {
      candidate.clamps[clamp].power_raw_by_phase[phase] = packet.power[clamp].phase[phase];
    }
  }

  *frame = candidate;
  return true;
}

}  // namespace emporiavue
}  // namespace esphome
