#include "emporiavue.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>
#include <limits>

namespace esphome {
namespace emporiavue {

static constexpr uint16_t SPI_RAW_FRAME_SIZE = 1024;
static constexpr uint8_t SPI_RAW_FRAME_VERSION = 1;
static constexpr uint8_t SPI_RAW_FRAME_TYPE_RAW_SAMPLES = 1;
static constexpr uint16_t SPI_RAW_FRAME_PAYLOAD_SIZE = 1008;
static constexpr uint16_t SPI_RAW_FRAME_HEADER_SIZE = 12;
static constexpr uint8_t SPI_RAW_SCAN_SIZE = 18;
static constexpr uint8_t SPI_RAW_SCAN_COUNT = 56;
static constexpr float SPI_SAMPLE_TIMEBASE_HZ = 16000000.0f;
static constexpr uint16_t SPI_RX_RECOVERY_INVALID_STREAK = 64;
static constexpr uint8_t SPI_RX_SOFT_RECOVERIES_BEFORE_SAMD_RESET = 3;
static constexpr uint32_t SPI_RX_SAMD_RESET_MIN_INTERVAL_MS = 2000;
static constexpr uint16_t SPI_RX_SAMD_RESET_BOOT_DELAY_MS = 20;
static constexpr uint32_t SPI_MAIN_SAMPLE_COUNT = 12987;
static constexpr uint16_t SPI_REFERENCE_WINDOW_MS = 500;
static constexpr uint32_t SPI_MAIN_RMS_SCALE_NUMERATOR = 100;
static constexpr uint32_t SPI_CURRENT_RMS_SCALE_NUMERATOR = 100;
static constexpr uint32_t SPI_POWER_SCALE_MULTIPLIER = 10;
static constexpr uint8_t SPI_ADC_OFFSET_CHANNEL_COUNT = 22;
static constexpr uint8_t SPI_ADC_OFFSET_Q_SHIFT = 8;
static constexpr int32_t SPI_ADC_OFFSET_INVALID_MIN = -1999;
static constexpr uint8_t SPI_ADC_OFFSET_STARTUP_WINDOWS = 4;
static constexpr uint8_t SPI_ADC_OFFSET_SMOOTHING_SHIFT = 4;
static constexpr int32_t SPI_ADC_OFFSET_MAX_STEP = 96;
static constexpr uint16_t SPI_MIN_METERING_SAMPLES = 512;
static constexpr uint16_t SPI_MAX_METERING_SAMPLES = SPI_MAIN_SAMPLE_COUNT;
static_assert(SPI_RAW_SCAN_SIZE * SPI_RAW_SCAN_COUNT == SPI_RAW_FRAME_PAYLOAD_SIZE,
              "SPI raw scan payload must fill the 1024-byte frame payload");
static constexpr uint8_t SPI_MUX_TABLE[16] = {12, 4, 13, 5, 14, 6, 11, 3, 15, 7, 18, 10, 16, 8, 17, 9};
static constexpr uint32_t SPI_CRC32_NIBBLE_TABLE[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
};

void EmporiaVueComponent::publish_spi_diagnostics_() {
  if (this->diag_frame_errors_sensor_ != nullptr) {
    this->diag_frame_errors_sensor_->publish_state(
        static_cast<float>(this->spi_rx_sync_errors_ + this->spi_rx_crc_errors_));
  }
  if (this->diag_transfer_errors_sensor_ != nullptr) {
    this->diag_transfer_errors_sensor_->publish_state(static_cast<float>(this->spi_rx_queue_errors_));
  }
  if (this->diag_frame_overruns_sensor_ != nullptr) {
    this->diag_frame_overruns_sensor_->publish_state(static_cast<float>(this->spi_rx_frame_gaps_));
  }
  if (this->diag_recoveries_sensor_ != nullptr &&
      (!this->spi_diag_recoveries_published_ ||
       this->spi_diag_published_recoveries_ != this->spi_rx_recoveries_)) {
    this->diag_recoveries_sensor_->publish_state(static_cast<float>(this->spi_rx_recoveries_));
    this->spi_diag_recoveries_published_ = true;
    this->spi_diag_published_recoveries_ = this->spi_rx_recoveries_;
  }
  if (this->diag_last_frame_samples_sensor_ != nullptr) {
    this->diag_last_frame_samples_sensor_->publish_state(static_cast<float>(this->spi_last_frame_samples_));
  }
  if (this->diag_sample_rate_sensor_ != nullptr && this->spi_sample_rate_hz_ > 1.0f) {
    this->diag_sample_rate_sensor_->publish_state(this->spi_sample_rate_hz_);
  }
}

void EmporiaVueComponent::setup_spi_receiver_(bool reset_statistics) {
  if (this->runtime_mode_ != RuntimeMode::SPI) {
    return;
  }
  if (!this->firmware_mode_matches_runtime_()) {
    this->start_firmware_mode_mismatch_log_();
    return;
  }

#ifdef USE_ESP32
  if (this->spi_receiver_started_) {
    if (this->spi_rx_task_handle_ != nullptr || !this->spi_rx_task_stop_) {
      return;
    }
    this->reset_spi_metering_state_();
    this->spi_rx_task_stop_ = false;
    this->spi_rx_inflight_ = 0;
    for (uint8_t index = 0; index < SPI_RX_QUEUE_SIZE; index++) {
      if (!this->queue_spi_receive_(index)) {
        this->spi_rx_task_stop_ = true;
        return;
      }
    }
    const BaseType_t task_result =
        xTaskCreate(EmporiaVueComponent::spi_rx_task_trampoline_, "samd_spi_rx", 4096, this, 5,
                    &this->spi_rx_task_handle_);
    if (task_result != pdPASS) {
      ESP_LOGE(TAG, "SAMD09 SPI receiver task restart failed");
      this->spi_rx_task_stop_ = true;
      return;
    }
    ESP_LOGI(TAG, "SAMD09 SPI receiver resumed");
    return;
  }

  spi_bus_config_t bus_config{};
  bus_config.mosi_io_num = this->spi_data_pin_;
  bus_config.miso_io_num = -1;
  bus_config.sclk_io_num = this->spi_clk_pin_;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.max_transfer_sz = SPI_RAW_FRAME_SIZE;

  spi_slave_interface_config_t slave_config{};
  slave_config.spics_io_num = this->spi_frame_pin_;
  slave_config.flags = 0;
  slave_config.queue_size = SPI_RX_QUEUE_SIZE;
  slave_config.mode = 0;

  esp_err_t err = spi_slave_initialize(this->spi_host_, &bus_config, &slave_config, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SAMD09 SPI receiver init failed: %d", static_cast<int>(err));
    return;
  }

  auto cleanup_spi_init = [this]() {
    spi_slave_free(this->spi_host_);
    if (this->spi_metering_queue_ != nullptr) {
      vQueueDelete(this->spi_metering_queue_);
      this->spi_metering_queue_ = nullptr;
    }
    for (auto *&buffer : this->spi_rx_buffers_) {
      if (buffer != nullptr) {
        heap_caps_free(buffer);
        buffer = nullptr;
      }
    }
    std::memset(this->spi_transactions_, 0, sizeof(this->spi_transactions_));
  };

  this->spi_metering_queue_ = xQueueCreate(1, sizeof(MeteringFrame));
  if (this->spi_metering_queue_ == nullptr) {
    ESP_LOGE(TAG, "SAMD09 SPI metering queue allocation failed");
    cleanup_spi_init();
    return;
  }
  this->reset_spi_metering_state_();

  for (uint8_t index = 0; index < SPI_RX_QUEUE_SIZE; index++) {
    this->spi_rx_buffers_[index] = static_cast<uint8_t *>(heap_caps_malloc(SPI_RAW_FRAME_SIZE, MALLOC_CAP_DMA));
    if (this->spi_rx_buffers_[index] == nullptr) {
      ESP_LOGE(TAG, "SAMD09 SPI receiver buffer allocation failed");
      cleanup_spi_init();
      return;
    }
    if (!this->queue_spi_receive_(index)) {
      cleanup_spi_init();
      return;
    }
  }

  this->spi_rx_inflight_ = 0;
  this->spi_rx_task_stop_ = false;
  if (reset_statistics) {
    this->spi_rx_frames_ = 0;
    this->spi_rx_sync_errors_ = 0;
    this->spi_rx_crc_errors_ = 0;
    this->spi_rx_queue_errors_ = 0;
    this->spi_rx_frame_gaps_ = 0;
    this->spi_rx_recoveries_ = 0;
    this->spi_rx_last_sequence_ = 0;
    this->spi_rx_waiting_for_valid_after_recovery_ = false;
    this->spi_rx_recoveries_since_valid_ = 0;
    this->spi_rx_last_recovery_attempts_ = 0;
    this->spi_rx_last_samd_reset_ms_ = 0;
    this->spi_diag_recoveries_published_ = false;
    this->spi_diag_published_recoveries_ = 0;
    this->spi_last_diagnostics_publish_ms_ = 0;
  }
  this->spi_rx_last_flags_ = 0;
  this->spi_rx_last_sample_counter_ = 0;
  this->spi_last_frame_samples_ = 0;
  this->spi_rx_invalid_streak_ = 0;
  this->spi_rx_recover_requested_ = false;
  this->spi_rx_logged_status_valid_ = reset_statistics;
  this->spi_rx_logged_sync_errors_ = this->spi_rx_sync_errors_;
  this->spi_rx_logged_crc_errors_ = this->spi_rx_crc_errors_;
  this->spi_rx_logged_queue_errors_ = this->spi_rx_queue_errors_;
  this->spi_rx_logged_frame_gaps_ = this->spi_rx_frame_gaps_;
  this->spi_rx_logged_recoveries_ = this->spi_rx_recoveries_;
  this->spi_rx_logged_flags_ = this->spi_rx_last_flags_;
  this->spi_receiver_started_ = true;
  this->spi_rx_last_log_ms_ = millis();
  const BaseType_t task_result =
      xTaskCreate(EmporiaVueComponent::spi_rx_task_trampoline_, "samd_spi_rx", 4096, this, 5,
                  &this->spi_rx_task_handle_);
  if (task_result != pdPASS) {
    ESP_LOGE(TAG, "SAMD09 SPI receiver task start failed");
    this->spi_receiver_started_ = false;
    cleanup_spi_init();
    return;
  }

  ESP_LOGI(TAG, "SAMD09 SPI receiver started: clk=GPIO%d data=GPIO%d cs=GPIO%d frame_size=%u queue=%u",
           this->spi_clk_pin_, this->spi_data_pin_, this->spi_frame_pin_, static_cast<unsigned>(SPI_RAW_FRAME_SIZE),
           static_cast<unsigned>(SPI_RX_QUEUE_SIZE));
#else
  ESP_LOGE(TAG, "SAMD09 SPI receiver requires ESP32");
#endif
}

bool EmporiaVueComponent::stop_spi_receiver_(bool release_driver) {
  if (!this->spi_receiver_started_) {
    return true;
  }

#ifdef USE_ESP32
  this->spi_rx_task_stop_ = true;
  for (uint16_t wait = 0; wait < 500 && this->spi_rx_task_handle_ != nullptr; wait++) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  if (this->spi_rx_task_handle_ != nullptr) {
    ESP_LOGE(TAG, "SAMD09 SPI receiver task did not stop cleanly; keeping SPI driver allocated");
    return false;
  }
  if (!release_driver) {
    ESP_LOGD(TAG, "SAMD09 SPI receiver paused; keeping SPI driver allocated");
    return true;
  }
  const esp_err_t free_err = spi_slave_free(this->spi_host_);
  if (free_err != ESP_OK) {
    ESP_LOGE(TAG, "SAMD09 SPI receiver free failed: %d", static_cast<int>(free_err));
    return false;
  }
  if (this->spi_metering_queue_ != nullptr) {
    vQueueDelete(this->spi_metering_queue_);
    this->spi_metering_queue_ = nullptr;
  }
  for (auto *&buffer : this->spi_rx_buffers_) {
    if (buffer != nullptr) {
      heap_caps_free(buffer);
      buffer = nullptr;
    }
  }
  std::memset(this->spi_transactions_, 0, sizeof(this->spi_transactions_));
#endif
  this->spi_receiver_started_ = false;
  return true;
}

void EmporiaVueComponent::restart_spi_receiver_() {
  const uint16_t invalid_streak = this->spi_rx_invalid_streak_;
  this->spi_rx_recoveries_++;
  if (this->spi_rx_recoveries_since_valid_ < UINT8_MAX) {
    this->spi_rx_recoveries_since_valid_++;
  }
  this->spi_rx_last_recovery_attempts_ = this->spi_rx_recoveries_since_valid_;

  const uint32_t now = millis();
  const bool reset_samd =
      this->reset_pin_ != nullptr &&
      this->spi_rx_recoveries_since_valid_ >= SPI_RX_SOFT_RECOVERIES_BEFORE_SAMD_RESET &&
      now - this->spi_rx_last_samd_reset_ms_ >= SPI_RX_SAMD_RESET_MIN_INTERVAL_MS;

  ESP_LOGW(TAG,
           "SAMD09 SPI receiver recovery: %s after %" PRIu16
           " invalid frames (recoveries=%" PRIu32 ")",
           reset_samd ? "resetting SAMD09" : "resyncing ESP32 SPI", invalid_streak, this->spi_rx_recoveries_);

  this->spi_rx_invalid_streak_ = 0;
  this->spi_rx_recover_requested_ = false;
  this->reset_spi_metering_state_();
#ifdef USE_ESP32
  if (this->spi_metering_queue_ != nullptr) {
    xQueueReset(this->spi_metering_queue_);
  }
#endif

  if (!this->stop_spi_receiver_(true)) {
    return;
  }

  this->reset_spi_metering_state_();

  if (reset_samd) {
    this->reset_pin_->setup();
    this->assert_reset_();
    this->deassert_reset_();
    this->reset_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    delay(SPI_RX_SAMD_RESET_BOOT_DELAY_MS);
    this->spi_rx_last_samd_reset_ms_ = millis();
    this->spi_rx_recoveries_since_valid_ = 0;
    this->spi_rx_frames_ = 0;
    this->spi_rx_last_sequence_ = 0;
    this->spi_rx_last_sample_counter_ = 0;
  }

  this->spi_rx_waiting_for_valid_after_recovery_ = true;
  this->setup_spi_receiver_(false);
}

bool EmporiaVueComponent::queue_spi_receive_(uint8_t index) {
#ifdef USE_ESP32
  if (index >= SPI_RX_QUEUE_SIZE || this->spi_rx_buffers_[index] == nullptr) {
    return false;
  }
  std::memset(&this->spi_transactions_[index], 0, sizeof(this->spi_transactions_[index]));
  this->spi_transactions_[index].length = SPI_RAW_FRAME_SIZE * 8;
  this->spi_transactions_[index].rx_buffer = this->spi_rx_buffers_[index];
  this->spi_transactions_[index].user = reinterpret_cast<void *>(static_cast<uintptr_t>(index));

  const esp_err_t err = spi_slave_queue_trans(this->spi_host_, &this->spi_transactions_[index], 0);
  if (err != ESP_OK) {
    this->spi_rx_queue_errors_++;
    ESP_LOGD(TAG, "SAMD09 SPI queue failed: %d", static_cast<int>(err));
    return false;
  }
  this->spi_rx_inflight_++;
  return true;
#else
  return false;
#endif
}

#ifdef USE_ESP32
void EmporiaVueComponent::process_spi_transaction_(spi_slave_transaction_t *transaction) {
  if (transaction == nullptr) {
    return;
  }
  const auto index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(transaction->user));
  if (this->spi_rx_inflight_ > 0) {
    this->spi_rx_inflight_--;
  }
  uint32_t sequence = 0;
  uint32_t flags = 0;
  uint32_t sample_counter = 0;
  uint16_t sample_period_ticks = 0;
  if (transaction->trans_len == SPI_RAW_FRAME_SIZE * 8 &&
      this->validate_spi_frame_(static_cast<const uint8_t *>(transaction->rx_buffer), &sequence, &flags,
                                &sample_counter, &sample_period_ticks)) {
    if (this->spi_rx_frames_ > 0) {
      const uint16_t last_sequence = static_cast<uint16_t>(this->spi_rx_last_sequence_);
      const uint16_t current_sequence = static_cast<uint16_t>(sequence);
      const uint16_t sequence_delta = static_cast<uint16_t>(current_sequence - last_sequence);
      if (sequence_delta > 1) {
        this->spi_rx_frame_gaps_ += static_cast<uint32_t>(sequence_delta - 1);
      }
    }
    this->spi_rx_frames_++;
    if (this->spi_rx_waiting_for_valid_after_recovery_) {
      ESP_LOGI(TAG,
               "SAMD09 SPI receiver recovered after resync, next valid seq=%" PRIu32
               " (recoveries_since_valid=%u)",
               sequence, static_cast<unsigned>(this->spi_rx_last_recovery_attempts_));
      this->spi_rx_waiting_for_valid_after_recovery_ = false;
      this->spi_rx_last_recovery_attempts_ = 0;
    }
    this->spi_rx_recoveries_since_valid_ = 0;
    this->spi_rx_last_sequence_ = sequence;
    this->spi_rx_last_flags_ = flags;
    this->spi_rx_last_sample_counter_ = sample_counter;
    this->spi_sample_rate_hz_ = SPI_SAMPLE_TIMEBASE_HZ / static_cast<float>(sample_period_ticks);
    this->spi_rx_invalid_streak_ = 0;
    this->decode_spi_raw_frame_(static_cast<const uint8_t *>(transaction->rx_buffer), sequence, flags, sample_counter);
  } else {
    if (this->spi_rx_frames_ == 0) {
      this->spi_rx_sync_errors_++;
    } else {
      this->spi_rx_crc_errors_++;
    }
    if (this->spi_rx_frames_ > 0 && this->spi_rx_invalid_streak_ < UINT16_MAX) {
      this->spi_rx_invalid_streak_++;
      if (this->spi_rx_invalid_streak_ >= SPI_RX_RECOVERY_INVALID_STREAK) {
        this->spi_rx_recover_requested_ = true;
      }
    }
  }
  if (!this->spi_rx_task_stop_) {
    this->queue_spi_receive_(index);
  }
}

void EmporiaVueComponent::spi_rx_task_trampoline_(void *arg) {
  static_cast<EmporiaVueComponent *>(arg)->spi_rx_task_();
}

void EmporiaVueComponent::spi_rx_task_() {
  while (!this->spi_rx_task_stop_ || this->spi_rx_inflight_ > 0) {
    spi_slave_transaction_t *transaction = nullptr;
    const esp_err_t err = spi_slave_get_trans_result(this->spi_host_, &transaction, pdMS_TO_TICKS(20));
    if (err == ESP_OK) {
      this->process_spi_transaction_(transaction);
    } else if (err != ESP_ERR_TIMEOUT && !this->spi_rx_task_stop_) {
      this->spi_rx_queue_errors_++;
      ESP_LOGD(TAG, "SAMD09 SPI get result failed: %d", static_cast<int>(err));
    }
  }
  this->spi_rx_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}
#endif

bool EmporiaVueComponent::validate_spi_frame_(const uint8_t *frame, uint32_t *sequence, uint32_t *flags,
                                              uint32_t *sample_counter, uint16_t *sample_period_ticks) const {
  if (frame[0] != SPI_RAW_FRAME_VERSION || frame[1] != SPI_RAW_FRAME_TYPE_RAW_SAMPLES) {
    return false;
  }

  const uint16_t payload_length = static_cast<uint16_t>(frame[2]) | (static_cast<uint16_t>(frame[3]) << 8);
  if (payload_length != SPI_RAW_FRAME_PAYLOAD_SIZE) {
    return false;
  }

  uint32_t crc = 0xFFFFFFFFUL;
  for (uint16_t index = 0; index < SPI_RAW_FRAME_SIZE - sizeof(uint32_t); index++) {
    crc ^= frame[index];
    crc = (crc >> 4) ^ SPI_CRC32_NIBBLE_TABLE[crc & 0x0FUL];
    crc = (crc >> 4) ^ SPI_CRC32_NIBBLE_TABLE[crc & 0x0FUL];
  }
  crc = ~crc;
  const uint32_t expected_crc = static_cast<uint32_t>(frame[SPI_RAW_FRAME_SIZE - 4]) |
                                (static_cast<uint32_t>(frame[SPI_RAW_FRAME_SIZE - 3]) << 8) |
                                (static_cast<uint32_t>(frame[SPI_RAW_FRAME_SIZE - 2]) << 16) |
                                (static_cast<uint32_t>(frame[SPI_RAW_FRAME_SIZE - 1]) << 24);
  if (crc != expected_crc) {
    return false;
  }

  *sequence = static_cast<uint16_t>(frame[4]) | (static_cast<uint16_t>(frame[5]) << 8);
  *flags = static_cast<uint16_t>(frame[6]) | (static_cast<uint16_t>(frame[7]) << 8);
  *sample_counter = static_cast<uint32_t>(frame[8]) | (static_cast<uint32_t>(frame[9]) << 8) |
                    (static_cast<uint32_t>(frame[10]) << 16) | (static_cast<uint32_t>(frame[11]) << 24);
  if (sample_period_ticks != nullptr) {
    const uint16_t reserved_low = frame[SPI_RAW_FRAME_HEADER_SIZE + 17];
    const uint16_t reserved_high = frame[SPI_RAW_FRAME_HEADER_SIZE + SPI_RAW_SCAN_SIZE + 17];
    *sample_period_ticks = static_cast<uint16_t>(reserved_low | (reserved_high << 8));
    if (*sample_period_ticks == 0) {
      return false;
    }
  }
  return true;
}

void EmporiaVueComponent::reset_spi_metering_state_() {
  this->spi_metering_accumulator_ = SpiMeteringAccumulator{};
  this->spi_metering_window_synced_ = false;
  std::memset(this->spi_raw_scan_ring_, 0, sizeof(this->spi_raw_scan_ring_));
  this->spi_raw_scan_ring_index_ = 0;
  this->spi_raw_scan_ring_count_ = 0;
  this->spi_expected_sample_counter_valid_ = false;
  this->spi_expected_sample_counter_ = 0;
  std::fill(this->spi_last_cross_sample_valid_, this->spi_last_cross_sample_valid_ + 3, false);
  std::fill(this->spi_last_cross_sample_, this->spi_last_cross_sample_ + 3, 0.0f);
  this->spi_line1_period_valid_ = false;
  this->spi_line1_period_samples_ = 0.0f;
  std::fill(this->spi_last_voltage_sample_valid_, this->spi_last_voltage_sample_valid_ + 3, false);
  std::fill(this->spi_last_voltage_difference_, this->spi_last_voltage_difference_ + 3, 0);
  std::fill(this->spi_last_voltage_sample_counter_, this->spi_last_voltage_sample_counter_ + 3, 0);
  std::fill(this->spi_pending_cross_sample_valid_, this->spi_pending_cross_sample_valid_ + 3, false);
  std::fill(this->spi_pending_cross_sample_, this->spi_pending_cross_sample_ + 3, 0.0f);
  this->spi_sample_rate_hz_ = 0.0f;
  this->spi_last_frame_samples_ = 0;
  std::fill(this->spi_cycle_state_, this->spi_cycle_state_ + 3, 0);
}

int16_t EmporiaVueComponent::sanitize_spi_adc_offset_(int32_t average) {
  if (average < SPI_ADC_OFFSET_INVALID_MIN) {
    return 0;
  }
  if (average > INT16_MAX) {
    return INT16_MAX;
  }
  if (average < INT16_MIN) {
    return INT16_MIN;
  }
  return static_cast<int16_t>(average);
}

uint16_t EmporiaVueComponent::scale_spi_rms_(uint64_t sum, uint32_t numerator, uint32_t denominator) {
  if (denominator == 0 || sum == 0) {
    return 0;
  }
  const double scaled = static_cast<double>(sum) * static_cast<double>(numerator) / static_cast<double>(denominator);
  if (scaled <= 0.0) {
    return 0;
  }
  const double root = std::sqrt(scaled);
  if (root >= 65535.0) {
    return 65535;
  }
  return static_cast<uint16_t>(root);
}

int32_t EmporiaVueComponent::scale_spi_power_(int64_t raw, uint32_t multiplier, uint32_t denominator) {
  if (denominator == 0 || raw == 0) {
    return 0;
  }
  const int64_t scaled = raw * static_cast<int64_t>(multiplier);
  const int64_t value = scaled / static_cast<int64_t>(denominator);
  if (value > static_cast<int64_t>(INT32_MAX)) {
    return INT32_MAX;
  }
  if (value < static_cast<int64_t>(INT32_MIN)) {
    return INT32_MIN;
  }
  return static_cast<int32_t>(value);
}

uint32_t EmporiaVueComponent::spi_metering_target_samples_() const {
  const uint32_t interval_ms = this->metering_interval_ms_ == 0 ? SPI_REFERENCE_WINDOW_MS : this->metering_interval_ms_;
  uint32_t samples = (SPI_MAIN_SAMPLE_COUNT * interval_ms + (SPI_REFERENCE_WINDOW_MS / 2)) / SPI_REFERENCE_WINDOW_MS;
  if (samples < SPI_MIN_METERING_SAMPLES) {
    samples = SPI_MIN_METERING_SAMPLES;
  }
  if (samples > SPI_MAX_METERING_SAMPLES) {
    samples = SPI_MAX_METERING_SAMPLES;
  }
  return samples;
}

uint16_t EmporiaVueComponent::spi_metering_target_periods_(float period_samples) const {
  if (period_samples <= 0.0f) {
    return 2;
  }
  const float target_samples = static_cast<float>(this->spi_metering_target_samples_());
  uint32_t periods = static_cast<uint32_t>((target_samples / period_samples) + 0.5f);
  if (periods < 2) {
    periods = 2;
  }
  if (periods > 64) {
    periods = 64;
  }
  return static_cast<uint16_t>(periods);
}

void EmporiaVueComponent::decode_spi_raw_frame_(const uint8_t *frame, uint32_t sequence, uint32_t flags,
                                                uint32_t sample_counter) {
  if (frame == nullptr) {
    return;
  }

  if (this->spi_expected_sample_counter_valid_ && sample_counter != this->spi_expected_sample_counter_) {
    this->reset_spi_metering_state_();
  }
  this->spi_expected_sample_counter_valid_ = true;
  this->spi_expected_sample_counter_ = sample_counter + SPI_RAW_SCAN_COUNT;

  const uint8_t *payload = frame + SPI_RAW_FRAME_HEADER_SIZE;
  for (uint8_t scan_index = 0; scan_index < SPI_RAW_SCAN_COUNT; scan_index++) {
    SpiRawScan scan{};
    const uint8_t *scan_data = payload + static_cast<uint16_t>(scan_index) * SPI_RAW_SCAN_SIZE;
    auto read_i16 = [](const uint8_t *data, uint8_t offset) -> int16_t {
      const uint16_t raw = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
      return static_cast<int16_t>(raw);
    };
    for (uint8_t value_index = 0; value_index < 8; value_index++) {
      scan.value[value_index] = read_i16(scan_data, static_cast<uint8_t>(value_index * 2));
    }
    scan.sample_counter = sample_counter + scan_index;
    scan.mux_index = scan_data[16];

    this->process_spi_raw_scan_(scan);
    auto &acc = this->spi_metering_accumulator_;
    if (this->spi_metering_window_synced_ && acc.line1_period_count != 0) {
      const float period_samples = acc.cycle_count[0] == 0 ? 0.0f : acc.cycle_sum[0] / acc.cycle_count[0];
      if (acc.line1_period_count >= this->spi_metering_target_periods_(period_samples)) {
        this->finish_spi_metering_window_(sequence, flags);
      }
    }
  }
}

void EmporiaVueComponent::process_spi_raw_scan_(const SpiRawScan &scan) {
  this->spi_raw_scan_ring_[this->spi_raw_scan_ring_index_] = scan;
  const uint8_t current_index = this->spi_raw_scan_ring_index_;
  this->spi_raw_scan_ring_index_ = static_cast<uint8_t>((this->spi_raw_scan_ring_index_ + 1) % 6);
  if (this->spi_raw_scan_ring_count_ < 6) {
    this->spi_raw_scan_ring_count_++;
  }

  const uint8_t max_delay = std::max(this->spi_main_current_delay_, this->spi_mux_current_delay_);
  if (this->spi_raw_scan_ring_count_ <= max_delay) {
    return;
  }

  const uint8_t main_current_index = static_cast<uint8_t>((current_index + 6 - this->spi_main_current_delay_) % 6);
  const uint8_t mux_current_index = static_cast<uint8_t>((current_index + 6 - this->spi_mux_current_delay_) % 6);
  const SpiRawScan &main_current_scan = this->spi_raw_scan_ring_[main_current_index];
  const SpiRawScan &mux_current_scan = this->spi_raw_scan_ring_[mux_current_index];
  auto &acc = this->spi_metering_accumulator_;

  int32_t voltage_differences[3]{};
  for (uint8_t phase = 0; phase < 3; phase++) {
    voltage_differences[phase] = static_cast<int32_t>(scan.value[phase * 2]) - this->spi_adc_offsets_[phase];
  }

  bool sync_window_after_scan = false;

  for (uint8_t phase = 0; phase < 3; phase++) {
    const int32_t voltage_difference = voltage_differences[phase];
    auto interpolated_crossing_sample = [this, phase, voltage_difference, &scan]() -> float {
      if (this->spi_last_voltage_sample_valid_[phase]) {
        const int32_t previous_difference = this->spi_last_voltage_difference_[phase];
        const int32_t delta = voltage_difference - previous_difference;
        if (previous_difference < 0 && delta > 0) {
          const float fraction =
              std::max(0.0f, std::min(1.0f, static_cast<float>(-previous_difference) / static_cast<float>(delta)));
          return static_cast<float>(this->spi_last_voltage_sample_counter_[phase]) + fraction;
        }
      }
      return static_cast<float>(scan.sample_counter);
    };

    uint8_t &cycle_state = this->spi_cycle_state_[phase];
    if (cycle_state == 43) {
      if (voltage_difference < 1) {
        this->spi_pending_cross_sample_valid_[phase] = false;
        cycle_state = 40;
      } else {
        const float crossing_sample = this->spi_pending_cross_sample_valid_[phase]
                                          ? this->spi_pending_cross_sample_[phase]
                                          : interpolated_crossing_sample();
        this->spi_pending_cross_sample_valid_[phase] = false;
        if (phase == 0) {
          if (this->spi_last_cross_sample_valid_[0]) {
            const float period = crossing_sample - this->spi_last_cross_sample_[0];
            if (period >= 300 && period <= 700) {
              this->spi_line1_period_samples_ = period;
              this->spi_line1_period_valid_ = true;
              if (this->spi_metering_window_synced_) {
                acc.cycle_sum[0] += period;
                acc.cycle_count[0]++;
                acc.line1_period_count++;
              } else {
                sync_window_after_scan = true;
              }
            }
          }
          this->spi_last_cross_sample_[0] = crossing_sample;
          this->spi_last_cross_sample_valid_[0] = true;
        } else if (this->spi_line1_period_valid_ && this->spi_last_cross_sample_valid_[0]) {
          const float period = this->spi_line1_period_samples_;
          float offset = crossing_sample - this->spi_last_cross_sample_[0];
          if (period > 0.0f) {
            offset = std::fmod(offset, period);
            if (offset < 0.0f) {
              offset += period;
            }
          }
          acc.cycle_sum[phase] += offset;
          acc.cycle_count[phase]++;
          this->spi_last_cross_sample_[phase] = crossing_sample;
          this->spi_last_cross_sample_valid_[phase] = true;
        }
        cycle_state = 0;
      }
    } else if (cycle_state < 40) {
      if (voltage_difference >= -100) {
        this->spi_pending_cross_sample_valid_[phase] = false;
        cycle_state = 0;
      } else {
        cycle_state++;
      }
    } else if (voltage_difference < 1) {
      this->spi_pending_cross_sample_valid_[phase] = false;
      cycle_state = 40;
    } else {
      if (cycle_state == 40) {
        this->spi_pending_cross_sample_[phase] = interpolated_crossing_sample();
        this->spi_pending_cross_sample_valid_[phase] = true;
      }
      cycle_state++;
    }
    this->spi_last_voltage_difference_[phase] = voltage_difference;
    this->spi_last_voltage_sample_counter_[phase] = scan.sample_counter;
    this->spi_last_voltage_sample_valid_[phase] = true;
  }

  if (sync_window_after_scan) {
    this->spi_metering_accumulator_ = SpiMeteringAccumulator{};
    this->spi_metering_window_synced_ = true;
    return;
  }

  if (!this->spi_metering_window_synced_) {
    return;
  }

  for (uint8_t phase = 0; phase < 3; phase++) {
    const int32_t voltage_difference = voltage_differences[phase];
    const uint8_t voltage_index = static_cast<uint8_t>(phase * 2);
    const uint8_t current_index_for_phase = static_cast<uint8_t>(voltage_index + 1);

    acc.voltage_sum[phase] += scan.value[voltage_index];
    acc.voltage_square_sum[phase] += static_cast<int64_t>(voltage_difference) * voltage_difference;

    acc.current_sum[phase] += main_current_scan.value[current_index_for_phase];
    const int32_t current_difference =
        static_cast<int32_t>(main_current_scan.value[current_index_for_phase]) - this->spi_adc_offsets_[3 + phase];
    acc.current_square_sum[phase] += static_cast<int64_t>(current_difference) * current_difference;
    for (uint8_t voltage_phase = 0; voltage_phase < 3; voltage_phase++) {
      acc.raw_power_sum[phase][voltage_phase] -=
          static_cast<int64_t>(current_difference) * voltage_differences[voltage_phase];
    }
  }

  if (scan.mux_index < 8) {
    for (uint8_t mux_side = 0; mux_side < 2; mux_side++) {
      const uint8_t internal_index = static_cast<uint8_t>(scan.mux_index * 2 + mux_side);
      const uint8_t clamp_index = static_cast<uint8_t>(3 + internal_index);
      const uint8_t offset_index = static_cast<uint8_t>(6 + internal_index);
      const int16_t current_raw = mux_current_scan.value[6 + mux_side];
      acc.current_sum[clamp_index] += current_raw;
      acc.mux_sample_count[internal_index]++;
      const int32_t current_difference = static_cast<int32_t>(current_raw) - this->spi_adc_offsets_[offset_index];
      acc.current_square_sum[clamp_index] += static_cast<int64_t>(current_difference) * current_difference;
      for (uint8_t voltage_phase = 0; voltage_phase < 3; voltage_phase++) {
        acc.raw_power_sum[clamp_index][voltage_phase] -=
            static_cast<int64_t>(current_difference) * voltage_differences[voltage_phase];
      }
    }
  }

  acc.sample_count++;
}

void EmporiaVueComponent::finish_spi_metering_window_(uint32_t sequence, uint32_t flags) {
  auto &acc = this->spi_metering_accumulator_;
  if (acc.sample_count == 0) {
    return;
  }
  this->spi_last_frame_samples_ = acc.sample_count;

  const uint32_t min_cycle_count = std::max<uint32_t>(2, static_cast<uint32_t>(acc.line1_period_count) / 2U);
  const uint32_t min_mux_samples = std::max<uint32_t>(8, acc.sample_count / 32U);
  bool phase_required[3]{false, false, false};
  auto mark_phase_required = [&phase_required](const MeteringPhaseConfig *phase) {
    if (phase != nullptr && phase->get_input_wire() < 3) {
      phase_required[phase->get_input_wire()] = true;
    }
  };
  for (const auto *phase : this->metering_phases_) {
    mark_phase_required(phase);
  }
  for (const auto *ct_clamp : this->metering_ct_clamps_) {
    if (ct_clamp == nullptr) {
      continue;
    }
    mark_phase_required(ct_clamp->get_phase());
    if (ct_clamp->is_line_pair()) {
      mark_phase_required(ct_clamp->get_line_pair_phase_b());
    }
  }
  for (const auto *virtual_line : this->metering_virtual_lines_) {
    if (virtual_line == nullptr) {
      continue;
    }
    mark_phase_required(virtual_line->get_line_a());
    mark_phase_required(virtual_line->get_line_b());
  }
  if (phase_required[1] || phase_required[2]) {
    phase_required[0] = true;
  }

  const char *invalid_reason = nullptr;
  uint32_t min_observed_mux_samples = std::numeric_limits<uint32_t>::max();
  auto mux_samples_for_port = [&acc](uint8_t port) -> uint32_t {
    if (port < 3) {
      return acc.sample_count;
    }
    for (uint8_t internal_index = 0; internal_index < 16; internal_index++) {
      if (SPI_MUX_TABLE[internal_index] == port) {
        return acc.mux_sample_count[internal_index];
      }
    }
    return 0;
  };

  if (acc.line1_period_count < 2) {
    invalid_reason = "short period window";
  } else if (acc.sample_count < SPI_MIN_METERING_SAMPLES) {
    invalid_reason = "short sample window";
  } else if (this->spi_sample_rate_hz_ <= 1.0f) {
    invalid_reason = "missing sample rate";
  } else if (phase_required[0] && acc.cycle_count[0] < min_cycle_count) {
    invalid_reason = "missing line 1 cycles";
  } else {
    for (uint8_t phase = 1; phase < 3; phase++) {
      if (phase_required[phase] && acc.cycle_count[phase] < min_cycle_count) {
        invalid_reason = phase == 1 ? "missing line 2 cycles" : "missing line 3 cycles";
        break;
      }
    }
  }

  if (invalid_reason == nullptr) {
    for (const auto *ct_clamp : this->metering_ct_clamps_) {
      if (ct_clamp == nullptr) {
        continue;
      }
      const uint8_t port = ct_clamp->get_input_port();
      if (port < 3 || port >= 19) {
        continue;
      }
      const uint32_t mux_samples = mux_samples_for_port(port);
      min_observed_mux_samples = std::min(min_observed_mux_samples, mux_samples);
      if (mux_samples < min_mux_samples) {
        invalid_reason = "missing circuit mux samples";
        break;
      }
    }
  }

  if (invalid_reason != nullptr) {
    const uint32_t now = millis();
    if ((now - this->spi_invalid_window_last_log_ms_) >= 5000U) {
      const uint32_t logged_min_mux_samples =
          min_observed_mux_samples == std::numeric_limits<uint32_t>::max() ? 0 : min_observed_mux_samples;
      ESP_LOGD(TAG,
               "SAMD09 SPI metering window rejected: %s samples=%" PRIu32 "/%" PRIu32
               " periods=%u cycles=%u/%u/%u min_cycles=%" PRIu32 " min_mux=%" PRIu32 "/%" PRIu32
               " sample_rate=%.1fHz",
               invalid_reason, acc.sample_count, this->spi_metering_target_samples_(),
               static_cast<unsigned>(acc.line1_period_count), static_cast<unsigned>(acc.cycle_count[0]),
               static_cast<unsigned>(acc.cycle_count[1]), static_cast<unsigned>(acc.cycle_count[2]),
               min_cycle_count, logged_min_mux_samples, min_mux_samples, this->spi_sample_rate_hz_);
      this->spi_invalid_window_last_log_ms_ = now;
    }
    this->spi_metering_accumulator_ = SpiMeteringAccumulator{};
    return;
  }

  auto update_offset = [this](uint8_t channel, int32_t window_average) {
    if (channel >= SPI_ADC_OFFSET_CHANNEL_COUNT) {
      return;
    }
    int32_t target_q8 = static_cast<int32_t>(sanitize_spi_adc_offset_(window_average)) << SPI_ADC_OFFSET_Q_SHIFT;
    if (this->spi_offset_warmup_windows_ < SPI_ADC_OFFSET_STARTUP_WINDOWS) {
      this->spi_adc_offset_estimate_q8_[channel] = target_q8;
    } else {
      const int32_t max_step_q8 = SPI_ADC_OFFSET_MAX_STEP << SPI_ADC_OFFSET_Q_SHIFT;
      int32_t delta_q8 = target_q8 - this->spi_adc_offset_estimate_q8_[channel];
      delta_q8 = std::max(-max_step_q8, std::min(max_step_q8, delta_q8));
      if (delta_q8 >= 0) {
        this->spi_adc_offset_estimate_q8_[channel] += delta_q8 >> SPI_ADC_OFFSET_SMOOTHING_SHIFT;
      } else {
        this->spi_adc_offset_estimate_q8_[channel] -= (-delta_q8) >> SPI_ADC_OFFSET_SMOOTHING_SHIFT;
      }
    }
    this->spi_adc_offsets_[channel] =
        static_cast<int16_t>(this->spi_adc_offset_estimate_q8_[channel] >> SPI_ADC_OFFSET_Q_SHIFT);
  };

  for (uint8_t phase = 0; phase < 3; phase++) {
    update_offset(phase, static_cast<int32_t>(acc.voltage_sum[phase] / static_cast<int32_t>(acc.sample_count)));
    update_offset(static_cast<uint8_t>(3 + phase),
                  static_cast<int32_t>(acc.current_sum[phase] / static_cast<int32_t>(acc.sample_count)));
  }
  for (uint8_t internal_index = 0; internal_index < 16; internal_index++) {
    const uint32_t mux_count = acc.mux_sample_count[internal_index];
    if (mux_count == 0) {
      continue;
    }
    update_offset(static_cast<uint8_t>(6 + internal_index),
                  static_cast<int32_t>(acc.current_sum[3 + internal_index] / static_cast<int32_t>(mux_count)));
  }
  if (this->spi_offset_warmup_windows_ < SPI_ADC_OFFSET_STARTUP_WINDOWS) {
    this->spi_offset_warmup_windows_++;
  }

  const float reference_rate =
      this->hardware_id_ == 3 ? VUE3_STOCK_CYCLE_TIMEBASE_HZ : VUE2_STOCK_CYCLE_TIMEBASE_HZ;
  const float sample_rate = this->spi_sample_rate_hz_;
  auto normalize_cycle_count = [reference_rate, sample_rate](float samples) -> uint16_t {
    if (samples <= 0.0f) {
      return 0;
    }
    const float normalized = samples * reference_rate / sample_rate;
    if (normalized <= 0.0f) {
      return 0;
    }
    if (normalized >= 65535.0f) {
      return 65535;
    }
    return static_cast<uint16_t>(normalized + 0.5f);
  };

  MeteringFrame frame{};
  frame.schema_version = 1;
  frame.transport = MeteringTransport::SPI;
  frame.sequence = sequence;
  frame.timestamp_ms = millis();
  frame.valid = true;
  frame.quality_flags = static_cast<uint8_t>(flags & 0xFFU);

  float line1_period_samples = 0.0f;
  if (acc.cycle_count[0] != 0) {
    line1_period_samples = acc.cycle_sum[0] / static_cast<float>(acc.cycle_count[0]);
    if (line1_period_samples > 0.0f) {
      frame.phases[0].phase_angle_degrees = 0.0f;
      if (this->spi_sample_rate_hz_ > 1.0f) {
        const float frequency = this->spi_sample_rate_hz_ / line1_period_samples;
        for (auto &phase : frame.phases) {
          phase.frequency_hz = frequency;
        }
      }
    }
  }

  for (uint8_t phase = 0; phase < 3; phase++) {
    frame.phases[phase].voltage_raw =
        scale_spi_rms_(static_cast<uint64_t>(acc.voltage_square_sum[phase]), SPI_MAIN_RMS_SCALE_NUMERATOR,
                       acc.sample_count);
    frame.clamps[phase].current_raw =
        scale_spi_rms_(static_cast<uint64_t>(acc.current_square_sum[phase]), SPI_CURRENT_RMS_SCALE_NUMERATOR,
                       acc.sample_count);
    for (uint8_t voltage_phase = 0; voltage_phase < 3; voltage_phase++) {
      frame.clamps[phase].power_raw_by_phase[voltage_phase] =
          scale_spi_power_(acc.raw_power_sum[phase][voltage_phase], SPI_POWER_SCALE_MULTIPLIER, acc.sample_count);
    }
    if (acc.cycle_count[phase] != 0) {
      const float cycle_samples = acc.cycle_sum[phase] / static_cast<float>(acc.cycle_count[phase]);
      frame.phases[phase].cycle_count_raw = normalize_cycle_count(cycle_samples);
      if (phase > 0 && line1_period_samples > 0.0f) {
        frame.phases[phase].phase_angle_degrees = cycle_samples * 360.0f / line1_period_samples;
      }
    }
  }

  for (uint8_t internal_index = 0; internal_index < 16; internal_index++) {
    const uint8_t output_port = SPI_MUX_TABLE[internal_index];
    if (output_port >= 19) {
      continue;
    }
    const uint32_t mux_count = acc.mux_sample_count[internal_index];
    if (mux_count == 0) {
      continue;
    }
    const uint8_t source_index = static_cast<uint8_t>(3 + internal_index);
    frame.clamps[output_port].current_raw =
        scale_spi_rms_(static_cast<uint64_t>(acc.current_square_sum[source_index]), SPI_CURRENT_RMS_SCALE_NUMERATOR,
                       mux_count);
    for (uint8_t voltage_phase = 0; voltage_phase < 3; voltage_phase++) {
      frame.clamps[output_port].power_raw_by_phase[voltage_phase] =
          scale_spi_power_(acc.raw_power_sum[source_index][voltage_phase], SPI_POWER_SCALE_MULTIPLIER, mux_count);
    }
  }

#ifdef USE_ESP32
  if (this->spi_metering_queue_ != nullptr) {
    xQueueOverwrite(this->spi_metering_queue_, &frame);
  }
#endif
  this->spi_metering_accumulator_ = SpiMeteringAccumulator{};
}

void EmporiaVueComponent::publish_queued_spi_metering_() {
#ifdef USE_ESP32
  if (this->spi_metering_queue_ == nullptr) {
    return;
  }
  MeteringFrame frame{};
  while (xQueueReceive(this->spi_metering_queue_, &frame, 0) == pdTRUE) {
    this->submit_metering_frame_(frame);
  }
#endif
}

void EmporiaVueComponent::process_spi_receiver_() {
  if (!this->spi_receiver_started_ || this->runtime_mode_ != RuntimeMode::SPI) {
    return;
  }

#ifdef USE_ESP32
  this->publish_queued_spi_metering_();

  if (this->spi_rx_recover_requested_) {
    this->restart_spi_receiver_();
    return;
  }

  const uint32_t now = millis();
  const bool status_changed =
      !this->spi_rx_logged_status_valid_ || this->spi_rx_sync_errors_ != this->spi_rx_logged_sync_errors_ ||
      this->spi_rx_crc_errors_ != this->spi_rx_logged_crc_errors_ ||
      this->spi_rx_queue_errors_ != this->spi_rx_logged_queue_errors_ ||
      this->spi_rx_frame_gaps_ != this->spi_rx_logged_frame_gaps_ ||
      this->spi_rx_recoveries_ != this->spi_rx_logged_recoveries_ ||
      this->spi_rx_last_flags_ != this->spi_rx_logged_flags_;
  if (status_changed && now - this->spi_rx_last_log_ms_ >= 1000) {
    this->spi_rx_last_log_ms_ = now;
    ESP_LOGD(TAG,
             "SAMD09 SPI rx status: sync_errors=%" PRIu32 " crc_errors=%" PRIu32 " queue_errors=%" PRIu32
             " seq_gaps=%" PRIu32 " recoveries=%" PRIu32 " inflight=%" PRIu16
             " flags=0x%04" PRIx32 " sample_counter=%" PRIu32,
             this->spi_rx_sync_errors_, this->spi_rx_crc_errors_, this->spi_rx_queue_errors_, this->spi_rx_frame_gaps_,
             this->spi_rx_recoveries_, this->spi_rx_inflight_, this->spi_rx_last_flags_,
             this->spi_rx_last_sample_counter_);
    this->spi_rx_logged_status_valid_ = true;
    this->spi_rx_logged_sync_errors_ = this->spi_rx_sync_errors_;
    this->spi_rx_logged_crc_errors_ = this->spi_rx_crc_errors_;
    this->spi_rx_logged_queue_errors_ = this->spi_rx_queue_errors_;
    this->spi_rx_logged_frame_gaps_ = this->spi_rx_frame_gaps_;
    this->spi_rx_logged_recoveries_ = this->spi_rx_recoveries_;
    this->spi_rx_logged_flags_ = this->spi_rx_last_flags_;
  }
  if (this->diagnostics_interval_ms_ != 0 &&
      now - this->spi_last_diagnostics_publish_ms_ >= this->diagnostics_interval_ms_) {
    this->spi_last_diagnostics_publish_ms_ = now;
    this->publish_spi_diagnostics_();
  }
#endif
}

}  // namespace emporiavue
}  // namespace esphome
