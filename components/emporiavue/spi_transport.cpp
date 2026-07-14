#include "emporiavue.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#include <freertos/task.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>
#include <limits>

namespace esphome {
namespace emporiavue {

static constexpr uint8_t SPI_VUE2_RAW_FRAME_VERSION = 1;
static constexpr uint8_t SPI_VUE2_RAW_FRAME_TYPE = 1;
static constexpr uint8_t SPI_VUE3_RAW_FRAME_VERSION = 2;
static constexpr uint8_t SPI_VUE3_RAW_FRAME_TYPE = 2;
static constexpr uint16_t SPI_RAW_FRAME_PAYLOAD_SIZE = 1008;
static constexpr uint16_t SPI_RAW_FRAME_HEADER_SIZE = 12;
static constexpr uint8_t SPI_RAW_SCAN_SIZE = 18;
static constexpr uint8_t SPI_RAW_SCAN_COUNT = 56;
static constexpr float SPI_SAMPLE_TIMEBASE_HZ = 16000000.0f;
static constexpr uint16_t SPI_RX_RECOVERY_INVALID_STREAK = 64;
static constexpr uint32_t SPI_RX_VALID_FRAME_TIMEOUT_MS = 2000;
static constexpr uint8_t SPI_RX_SOFT_RECOVERIES_BEFORE_SAMD_RESET = 3;
static constexpr uint32_t SPI_RX_SAMD_RESET_MIN_INTERVAL_MS = 2000;
static constexpr uint16_t SPI_RX_SAMD_RESET_BOOT_DELAY_MS = 20;
static constexpr uint32_t SPI_VUE2_MAIN_SAMPLE_COUNT = 12987;
static constexpr uint32_t SPI_VUE3_MAIN_SAMPLE_COUNT = 10000;
static constexpr uint16_t SPI_REFERENCE_WINDOW_MS = 500;
static constexpr uint32_t SPI_RX_TASK_STACK_SIZE = 4096;
static constexpr uint32_t SPI_METERING_TASK_STACK_SIZE = 8192;
static constexpr uint32_t SPI_MAIN_RMS_SCALE_NUMERATOR = 100;
static constexpr uint32_t SPI_CURRENT_RMS_SCALE_NUMERATOR = 100;
static constexpr uint32_t SPI_CURRENT_PEAK_SCALE_MULTIPLIER = 10;
static constexpr float SPI_VOLTAGE_STATISTIC_SCALE_MULTIPLIER = 100.0f;
static constexpr uint32_t SPI_POWER_SCALE_MULTIPLIER = 10;
static constexpr uint8_t SPI_ADC_OFFSET_CHANNEL_COUNT = 22;
static constexpr uint8_t SPI_ADC_OFFSET_Q_SHIFT = 8;
static constexpr int32_t SPI_ADC_OFFSET_INVALID_MIN = -1999;
static constexpr uint8_t SPI_ADC_OFFSET_STARTUP_WINDOWS = 4;
static constexpr uint8_t SPI_ADC_OFFSET_SMOOTHING_SHIFT = 4;
static constexpr int32_t SPI_ADC_OFFSET_MAX_STEP = 96;
static constexpr uint16_t SPI_MIN_METERING_SAMPLES = 512;
static constexpr uint16_t SPI_MAX_METERING_SAMPLES = SPI_VUE2_MAIN_SAMPLE_COUNT;
static constexpr float SPI_TWO_PI = 6.28318530717958647692f;
static constexpr float SPI_FUNDAMENTAL_RMS_COMPONENT_SCALE = 14.142135623730950488f;
static constexpr float SPI_MIN_VOLTAGE_FUNDAMENTAL_AMPLITUDE = 16.0f;
static constexpr uint16_t SPI_FRAME_FLAG_OVERRUN = 0x0001;
static constexpr uint16_t SPI_FRAME_FLAG_DMA_ERROR = 0x0002;
static constexpr uint16_t SPI_FRAME_FLAG_VOLTAGE_ERROR = 0x0004;
static constexpr uint16_t SPI_FRAME_ERROR_FLAG_MASK =
    SPI_FRAME_FLAG_OVERRUN | SPI_FRAME_FLAG_DMA_ERROR | SPI_FRAME_FLAG_VOLTAGE_ERROR;
static_assert(SPI_RAW_SCAN_SIZE * SPI_RAW_SCAN_COUNT == SPI_RAW_FRAME_PAYLOAD_SIZE,
              "SPI raw scan payload must fill the 1024-byte frame payload");
static constexpr uint8_t SPI_VUE2_MUX_TABLE[16] = {12, 4, 13, 5, 14, 6, 11, 3, 15, 7, 18, 10, 16, 8, 17, 9};
static constexpr uint8_t SPI_VUE3_MUX_TABLE[16] = {16, 8, 17, 9, 12, 4, 13, 5, 14, 6, 11, 3, 15, 7, 18, 10};

static uint8_t spi_mux_output_port(uint16_t hardware_id, uint8_t internal_index) {
  return hardware_id == 3 ? SPI_VUE3_MUX_TABLE[internal_index] : SPI_VUE2_MUX_TABLE[internal_index];
}
static constexpr uint32_t SPI_CRC32_NIBBLE_TABLE[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
};

static uint32_t integer_sqrt_u64(uint64_t value) {
  uint64_t result = 0;
  uint64_t bit = uint64_t{1} << 62U;
  while (bit > value) {
    bit >>= 2U;
  }
  while (bit != 0) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1U) + bit;
    } else {
      result >>= 1U;
    }
    bit >>= 2U;
  }
  return static_cast<uint32_t>(result);
}

void EmporiaVueComponent::publish_spi_diagnostics_() {
  if (this->diag_frame_errors_sensor_ != nullptr) {
    this->diag_frame_errors_sensor_->publish_state(
        static_cast<float>(this->spi_rx_sync_errors_ + this->spi_rx_crc_errors_));
  }
  if (this->diag_transfer_errors_sensor_ != nullptr) {
    this->diag_transfer_errors_sensor_->publish_state(
        static_cast<float>(this->spi_rx_queue_errors_ + this->spi_processing_overruns_ + this->spi_rx_dma_errors_));
  }
  if (this->diag_frame_overruns_sensor_ != nullptr) {
    this->diag_frame_overruns_sensor_->publish_state(static_cast<float>(this->spi_rx_samd_overruns_));
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
#ifdef USE_ESP32
  const size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t heap_minimum = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  const size_t heap_largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const bool heap_valid = heap_caps_check_integrity_all(true);
  const UBaseType_t loop_stack_free = uxTaskGetStackHighWaterMark(nullptr);
  UBaseType_t spi_rx_stack_free = 0;
  UBaseType_t spi_metering_stack_free = 0;
  const TaskHandle_t spi_rx_task = this->spi_rx_task_handle_;
  const TaskHandle_t spi_metering_task = this->spi_metering_task_handle_;
  if (spi_rx_task != nullptr) {
    spi_rx_stack_free = uxTaskGetStackHighWaterMark(spi_rx_task);
  }
  if (spi_metering_task != nullptr) {
    spi_metering_stack_free = uxTaskGetStackHighWaterMark(spi_metering_task);
  }
  const UBaseType_t spi_stack_free =
      spi_rx_stack_free == 0 ? spi_metering_stack_free
                             : (spi_metering_stack_free == 0 ? spi_rx_stack_free
                                                             : std::min(spi_rx_stack_free, spi_metering_stack_free));
  if (this->diag_heap_free_sensor_ != nullptr) {
    this->diag_heap_free_sensor_->publish_state(static_cast<float>(heap_free));
  }
  if (this->diag_heap_minimum_sensor_ != nullptr) {
    this->diag_heap_minimum_sensor_->publish_state(static_cast<float>(heap_minimum));
  }
  if (this->diag_heap_largest_block_sensor_ != nullptr) {
    this->diag_heap_largest_block_sensor_->publish_state(static_cast<float>(heap_largest_block));
  }
  if (this->diag_heap_corruption_sensor_ != nullptr) {
    this->diag_heap_corruption_sensor_->publish_state(!heap_valid);
  }
  if (this->diag_loop_stack_free_sensor_ != nullptr) {
    this->diag_loop_stack_free_sensor_->publish_state(static_cast<float>(loop_stack_free));
  }
  if (this->diag_spi_stack_free_sensor_ != nullptr && spi_stack_free != 0) {
    this->diag_spi_stack_free_sensor_->publish_state(static_cast<float>(spi_stack_free));
  }
  ESP_LOGI(TAG,
           "Runtime memory: heap_free=%u heap_min=%u largest_block=%u heap_valid=%s "
           "loop_stack_free=%u spi_stack_free=%u spi_rx_stack_free=%u spi_metering_stack_free=%u bytes",
           static_cast<unsigned>(heap_free), static_cast<unsigned>(heap_minimum),
           static_cast<unsigned>(heap_largest_block), YESNO(heap_valid), static_cast<unsigned>(loop_stack_free),
           static_cast<unsigned>(spi_stack_free), static_cast<unsigned>(spi_rx_stack_free),
           static_cast<unsigned>(spi_metering_stack_free));
#endif
}

void EmporiaVueComponent::setup_spi_receiver_(bool reset_statistics) {
  if (this->runtime_mode_ != RuntimeMode::SPI) {
    return;
  }
  if (!this->firmware_mode_matches_runtime_()) {
    this->start_firmware_mode_mismatch_log_();
    return;
  }

  this->configure_spi_analysis_requirements_();

#ifdef USE_ESP32
  if (this->spi_receiver_started_) {
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
    const esp_err_t disable_err = spi_slave_disable(this->spi_host_);
    if (disable_err != ESP_OK && disable_err != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "SAMD09 SPI receiver disable during cleanup failed: %d", static_cast<int>(disable_err));
    }
    const esp_err_t free_err = spi_slave_free(this->spi_host_);
    if (free_err != ESP_OK) {
      ESP_LOGE(TAG, "SAMD09 SPI receiver cleanup failed: %d", static_cast<int>(free_err));
      return;
    }
    if (this->spi_metering_queue_ != nullptr) {
      vQueueDelete(this->spi_metering_queue_);
      this->spi_metering_queue_ = nullptr;
    }
    if (this->spi_processing_ready_queue_ != nullptr) {
      vQueueDelete(this->spi_processing_ready_queue_);
      this->spi_processing_ready_queue_ = nullptr;
    }
    if (this->spi_processing_free_queue_ != nullptr) {
      vQueueDelete(this->spi_processing_free_queue_);
      this->spi_processing_free_queue_ = nullptr;
    }
    if (this->spi_processing_frames_ != nullptr) {
      heap_caps_free(this->spi_processing_frames_);
      this->spi_processing_frames_ = nullptr;
    }
    for (auto *&buffer : this->spi_rx_buffers_) {
      if (buffer != nullptr) {
        heap_caps_free(buffer);
        buffer = nullptr;
      }
    }
    std::memset(this->spi_transactions_, 0, sizeof(this->spi_transactions_));
    this->spi_rx_inflight_ = 0;
  };

  this->spi_metering_queue_ = xQueueCreate(1, sizeof(MeteringFrame));
  this->spi_processing_free_queue_ = xQueueCreate(SPI_PROCESSING_FRAME_COUNT, sizeof(SpiQueuedFrame *));
  this->spi_processing_ready_queue_ = xQueueCreate(SPI_PROCESSING_FRAME_COUNT, sizeof(SpiQueuedFrame *));
  if (this->spi_metering_queue_ == nullptr || this->spi_processing_free_queue_ == nullptr ||
      this->spi_processing_ready_queue_ == nullptr) {
    ESP_LOGE(TAG, "SAMD09 SPI queue allocation failed");
    cleanup_spi_init();
    return;
  }

  this->spi_processing_frames_ = static_cast<SpiQueuedFrame *>(
      heap_caps_calloc(SPI_PROCESSING_FRAME_COUNT, sizeof(SpiQueuedFrame), MALLOC_CAP_8BIT));
  if (this->spi_processing_frames_ == nullptr) {
    ESP_LOGE(TAG, "SAMD09 SPI processing frame allocation failed");
    cleanup_spi_init();
    return;
  }
  for (uint8_t index = 0; index < SPI_PROCESSING_FRAME_COUNT; index++) {
    SpiQueuedFrame *frame = &this->spi_processing_frames_[index];
    if (xQueueSend(this->spi_processing_free_queue_, &frame, 0) != pdTRUE) {
      ESP_LOGE(TAG, "SAMD09 SPI processing free queue initialization failed");
      cleanup_spi_init();
      return;
    }
  }

  this->reset_spi_metering_state_();
  this->spi_rx_inflight_ = 0;

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

  this->spi_rx_task_stop_ = false;
  this->spi_rx_force_stop_ = false;
  this->spi_metering_task_stop_ = false;
  if (reset_statistics) {
    this->spi_rx_frames_ = 0;
    this->spi_rx_sync_errors_ = 0;
    this->spi_rx_crc_errors_ = 0;
    this->spi_rx_queue_errors_ = 0;
    this->spi_processing_overruns_ = 0;
    this->spi_rx_dma_errors_ = 0;
    this->spi_rx_samd_overruns_ = 0;
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
  this->spi_rx_stall_recovery_ = false;
  this->spi_rx_logged_status_valid_ = reset_statistics;
  this->spi_rx_logged_sync_errors_ = this->spi_rx_sync_errors_;
  this->spi_rx_logged_crc_errors_ = this->spi_rx_crc_errors_;
  this->spi_rx_logged_queue_errors_ = this->spi_rx_queue_errors_;
  this->spi_rx_logged_processing_overruns_ = this->spi_processing_overruns_;
  this->spi_rx_logged_dma_errors_ = this->spi_rx_dma_errors_;
  this->spi_rx_logged_samd_overruns_ = this->spi_rx_samd_overruns_;
  this->spi_rx_logged_frame_gaps_ = this->spi_rx_frame_gaps_;
  this->spi_rx_logged_recoveries_ = this->spi_rx_recoveries_;
  this->spi_rx_logged_flags_ = this->spi_rx_last_flags_;
  this->spi_rx_last_log_ms_ = millis();
  this->spi_rx_last_valid_frame_ms_ = this->spi_rx_last_log_ms_;
  const BaseType_t metering_task_result =
      xTaskCreate(EmporiaVueComponent::spi_metering_task_trampoline_, "samd_metering", SPI_METERING_TASK_STACK_SIZE,
                  this, 2, &this->spi_metering_task_handle_);
  if (metering_task_result != pdPASS) {
    ESP_LOGE(TAG, "SAMD09 SPI metering task start failed");
    cleanup_spi_init();
    return;
  }

  this->spi_rx_task_running_ = true;
  const BaseType_t rx_task_result =
      xTaskCreate(EmporiaVueComponent::spi_rx_task_trampoline_, "samd_spi_rx", SPI_RX_TASK_STACK_SIZE, this, 5,
                  &this->spi_rx_task_handle_);
  if (rx_task_result != pdPASS) {
    this->spi_rx_task_running_ = false;
    ESP_LOGE(TAG, "SAMD09 SPI receiver task start failed");
    this->spi_metering_task_stop_ = true;
    for (uint16_t wait = 0; wait < 50 && this->spi_metering_task_handle_ != nullptr; wait++) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (this->spi_metering_task_handle_ != nullptr) {
      ESP_LOGE(TAG, "SAMD09 SPI metering task did not stop after receiver task start failure");
      return;
    }
    cleanup_spi_init();
    return;
  }
  this->spi_receiver_started_ = true;

  ESP_LOGI(TAG,
           "SAMD09 SPI pipeline started: clk=GPIO%d data=GPIO%d cs=GPIO%d frame_size=%u dma_queue=%u "
           "processing_queue=%u",
           this->spi_clk_pin_, this->spi_data_pin_, this->spi_frame_pin_, static_cast<unsigned>(SPI_RAW_FRAME_SIZE),
           static_cast<unsigned>(SPI_RX_QUEUE_SIZE), static_cast<unsigned>(SPI_PROCESSING_FRAME_COUNT));
#else
  ESP_LOGE(TAG, "SAMD09 SPI receiver requires ESP32");
#endif
}

bool EmporiaVueComponent::stop_spi_receiver_() {
  if (!this->spi_receiver_started_) {
    return true;
  }

#ifdef USE_ESP32
  this->spi_rx_task_stop_ = true;
  this->spi_metering_task_stop_ = true;
  for (uint16_t wait = 0; wait < 125 && this->spi_rx_task_handle_ != nullptr; wait++) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  bool driver_disabled = false;
  if (this->spi_rx_task_handle_ != nullptr) {
    ESP_LOGW(TAG, "SAMD09 SPI receiver stalled while stopping; forcing a clean driver restart");
    const esp_err_t disable_err = spi_slave_disable(this->spi_host_);
    if (disable_err != ESP_OK && disable_err != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "SAMD09 SPI receiver disable failed: %d", static_cast<int>(disable_err));
      return false;
    }
    driver_disabled = true;
    this->spi_rx_force_stop_ = true;
    for (uint16_t wait = 0; wait < 50 && this->spi_rx_task_handle_ != nullptr; wait++) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
  if (this->spi_rx_task_handle_ != nullptr) {
    ESP_LOGE(TAG, "SAMD09 SPI receiver task did not stop cleanly; keeping SPI driver allocated");
    return false;
  }
  for (uint16_t wait = 0; wait < 125 && this->spi_metering_task_handle_ != nullptr; wait++) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }
  if (this->spi_metering_task_handle_ != nullptr) {
    ESP_LOGE(TAG, "SAMD09 SPI metering task did not stop cleanly; keeping SPI driver allocated");
    return false;
  }
  if (!driver_disabled) {
    const esp_err_t disable_err = spi_slave_disable(this->spi_host_);
    if (disable_err != ESP_OK && disable_err != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "SAMD09 SPI receiver disable failed: %d", static_cast<int>(disable_err));
      return false;
    }
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
  if (this->spi_processing_ready_queue_ != nullptr) {
    vQueueDelete(this->spi_processing_ready_queue_);
    this->spi_processing_ready_queue_ = nullptr;
  }
  if (this->spi_processing_free_queue_ != nullptr) {
    vQueueDelete(this->spi_processing_free_queue_);
    this->spi_processing_free_queue_ = nullptr;
  }
  if (this->spi_processing_frames_ != nullptr) {
    heap_caps_free(this->spi_processing_frames_);
    this->spi_processing_frames_ = nullptr;
  }
  for (auto *&buffer : this->spi_rx_buffers_) {
    if (buffer != nullptr) {
      heap_caps_free(buffer);
      buffer = nullptr;
    }
  }
  std::memset(this->spi_transactions_, 0, sizeof(this->spi_transactions_));
  this->spi_rx_inflight_ = 0;
#endif
  this->spi_receiver_started_ = false;
  this->spi_rx_last_valid_frame_ms_ = 0;
  return true;
}

void EmporiaVueComponent::restart_spi_receiver_() {
  const uint16_t invalid_streak = this->spi_rx_invalid_streak_;
  const bool stalled = this->spi_rx_stall_recovery_;
  const uint32_t stalled_ms = stalled ? millis() - this->spi_rx_last_valid_frame_ms_ : 0;
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

  if (stalled) {
    ESP_LOGW(TAG,
             "SAMD09 SPI receiver recovery: %s after no valid frame for %" PRIu32
             " ms (recoveries=%" PRIu32 ")",
             reset_samd ? "resetting SAMD09" : "resyncing ESP32 SPI", stalled_ms, this->spi_rx_recoveries_);
  } else if (invalid_streak != 0) {
    ESP_LOGW(TAG,
             "SAMD09 SPI receiver recovery: %s after %" PRIu16
             " invalid frames (recoveries=%" PRIu32 ")",
             reset_samd ? "resetting SAMD09" : "resyncing ESP32 SPI", invalid_streak, this->spi_rx_recoveries_);
  } else {
    ESP_LOGW(TAG, "SAMD09 SPI receiver recovery: %s after SPI driver error (recoveries=%" PRIu32 ")",
             reset_samd ? "resetting SAMD09" : "resyncing ESP32 SPI", this->spi_rx_recoveries_);
  }

  if (!this->stop_spi_receiver_()) {
    this->spi_rx_recover_requested_ = true;
    return;
  }

  // The metering task exclusively owns the raw-scan rings and accumulator.
  // Both pipeline tasks are stopped before that state is cleared.
  this->spi_rx_invalid_streak_ = 0;
  this->spi_rx_recover_requested_ = false;
  this->spi_rx_stall_recovery_ = false;
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
  this->spi_rx_inflight_ = static_cast<uint16_t>(this->spi_rx_inflight_ + 1);
  return true;
#else
  return false;
#endif
}

#ifdef USE_ESP32
void EmporiaVueComponent::handoff_spi_transaction_(spi_slave_transaction_t *transaction) {
  if (transaction == nullptr) {
    return;
  }
  const auto index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(transaction->user));
  if (this->spi_rx_inflight_ > 0) {
    this->spi_rx_inflight_ = static_cast<uint16_t>(this->spi_rx_inflight_ - 1);
  }

  SpiQueuedFrame *frame = nullptr;
  // The transport task must never wait for metering. If every processing frame
  // is busy, drop this complete frame and immediately requeue its DMA buffer.
  if (this->spi_processing_free_queue_ != nullptr && this->spi_processing_ready_queue_ != nullptr &&
      xQueueReceive(this->spi_processing_free_queue_, &frame, 0) == pdTRUE && frame != nullptr) {
    frame->trans_len_bits = static_cast<uint16_t>(std::min<size_t>(transaction->trans_len, UINT16_MAX));
    std::memcpy(frame->data, transaction->rx_buffer, SPI_RAW_FRAME_SIZE);
    if (xQueueSend(this->spi_processing_ready_queue_, &frame, 0) != pdTRUE) {
      this->spi_processing_overruns_ = this->spi_processing_overruns_ + 1;
      if (xQueueSend(this->spi_processing_free_queue_, &frame, 0) != pdTRUE) {
        this->spi_rx_queue_errors_++;
        this->spi_rx_recover_requested_ = true;
      }
    }
  } else {
    this->spi_processing_overruns_ = this->spi_processing_overruns_ + 1;
  }

  if (!this->spi_rx_task_stop_ && !this->queue_spi_receive_(index)) {
    this->spi_rx_recover_requested_ = true;
  }
}

void EmporiaVueComponent::process_spi_frame_(const SpiQueuedFrame &frame) {
  uint32_t sequence = 0;
  uint32_t flags = 0;
  uint32_t sample_counter = 0;
  uint16_t sample_period_ticks = 0;
  const bool protocol_valid =
      frame.trans_len_bits == SPI_RAW_FRAME_SIZE * 8 &&
      this->validate_spi_frame_(frame.data, &sequence, &flags, &sample_counter, &sample_period_ticks);
  if (protocol_valid && (flags & SPI_FRAME_FLAG_OVERRUN) != 0) {
    this->spi_rx_samd_overruns_++;
  }
  const bool samd_sample_error =
      protocol_valid && (flags & (SPI_FRAME_FLAG_DMA_ERROR | SPI_FRAME_FLAG_VOLTAGE_ERROR)) != 0;
  if (protocol_valid && !samd_sample_error) {
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
    this->spi_rx_last_valid_frame_ms_ = millis();
    this->decode_spi_raw_frame_(frame.data, sequence, flags, sample_counter);
  } else {
    if (samd_sample_error) {
      this->spi_rx_dma_errors_++;
      this->spi_rx_last_flags_ = flags;
      this->reset_spi_metering_state_();
      if (this->spi_rx_invalid_streak_ == 0) {
        ESP_LOGW(TAG, "Discarding SAMD09 SPI frame seq=%" PRIu32 " after SAMD sample error flags=0x%04" PRIX32,
                 sequence, flags);
      }
    } else if (this->spi_rx_frames_ == 0) {
      this->spi_rx_sync_errors_++;
    } else {
      this->spi_rx_crc_errors_++;
    }
    if (this->spi_rx_invalid_streak_ < UINT16_MAX) {
      this->spi_rx_invalid_streak_ = static_cast<uint16_t>(this->spi_rx_invalid_streak_ + 1);
      if (this->spi_rx_invalid_streak_ >= SPI_RX_RECOVERY_INVALID_STREAK) {
        this->spi_rx_recover_requested_ = true;
      }
    }
  }
}

void EmporiaVueComponent::spi_rx_task_trampoline_(void *arg) {
  static_cast<EmporiaVueComponent *>(arg)->spi_rx_task_();
}

void EmporiaVueComponent::spi_rx_task_() {
  while (!this->spi_rx_task_stop_ || (!this->spi_rx_force_stop_ && this->spi_rx_inflight_ > 0)) {
    spi_slave_transaction_t *transaction = nullptr;
    const esp_err_t err = spi_slave_get_trans_result(this->spi_host_, &transaction, pdMS_TO_TICKS(20));
    if (err == ESP_OK) {
      this->handoff_spi_transaction_(transaction);
    } else if (err != ESP_ERR_TIMEOUT && !this->spi_rx_task_stop_) {
      this->spi_rx_queue_errors_++;
      this->spi_rx_recover_requested_ = true;
      ESP_LOGD(TAG, "SAMD09 SPI get result failed: %d", static_cast<int>(err));
    }
  }
  this->spi_rx_task_running_ = false;
  this->spi_rx_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}

void EmporiaVueComponent::spi_metering_task_trampoline_(void *arg) {
  static_cast<EmporiaVueComponent *>(arg)->spi_metering_task_();
}

void EmporiaVueComponent::spi_metering_task_() {
  while (true) {
    SpiQueuedFrame *frame = nullptr;
    const bool frame_ready = this->spi_processing_ready_queue_ != nullptr &&
                             xQueueReceive(this->spi_processing_ready_queue_, &frame, pdMS_TO_TICKS(20)) == pdTRUE;
    if (frame_ready && frame != nullptr) {
      this->process_spi_frame_(*frame);
      if (this->spi_processing_free_queue_ == nullptr ||
          xQueueSend(this->spi_processing_free_queue_, &frame, 0) != pdTRUE) {
        this->spi_rx_queue_errors_++;
        this->spi_rx_recover_requested_ = true;
      }
      continue;
    }

    const bool ready_queue_empty = this->spi_processing_ready_queue_ == nullptr ||
                                   uxQueueMessagesWaiting(this->spi_processing_ready_queue_) == 0;
    if (this->spi_metering_task_stop_ && !this->spi_rx_task_running_ && ready_queue_empty) {
      break;
    }
  }
  this->spi_metering_task_handle_ = nullptr;
  vTaskDelete(nullptr);
}
#endif

bool EmporiaVueComponent::validate_spi_frame_(const uint8_t *frame, uint32_t *sequence, uint32_t *flags,
                                              uint32_t *sample_counter, uint16_t *sample_period_ticks) const {
  const uint8_t expected_version = this->hardware_id_ == 3 ? SPI_VUE3_RAW_FRAME_VERSION : SPI_VUE2_RAW_FRAME_VERSION;
  const uint8_t expected_type = this->hardware_id_ == 3 ? SPI_VUE3_RAW_FRAME_TYPE : SPI_VUE2_RAW_FRAME_TYPE;
  if (frame[0] != expected_version || frame[1] != expected_type) {
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
  const uint16_t raw_flags = static_cast<uint16_t>(frame[6]) | (static_cast<uint16_t>(frame[7]) << 8);
  *flags = raw_flags & SPI_FRAME_ERROR_FLAG_MASK;
  *sample_counter = static_cast<uint32_t>(frame[8]) | (static_cast<uint32_t>(frame[9]) << 8) |
                    (static_cast<uint32_t>(frame[10]) << 16) | (static_cast<uint32_t>(frame[11]) << 24);
  if (sample_period_ticks != nullptr) {
    if (this->hardware_id_ == 3) {
      *sample_period_ticks = raw_flags >> 3;
    } else {
      const uint16_t reserved_low = frame[SPI_RAW_FRAME_HEADER_SIZE + 17];
      const uint16_t reserved_high = frame[SPI_RAW_FRAME_HEADER_SIZE + SPI_RAW_SCAN_SIZE + 17];
      *sample_period_ticks = static_cast<uint16_t>(reserved_low | (reserved_high << 8));
    }
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
  std::memset(this->spi_voltage_sample_ring_, 0, sizeof(this->spi_voltage_sample_ring_));
  this->spi_voltage_sample_ring_index_ = 0;
  this->spi_voltage_sample_ring_count_ = 0;
  this->spi_voltage_sample_ring_last_counter_ = 0;
  this->spi_expected_sample_counter_valid_ = false;
  this->spi_expected_sample_counter_ = 0;
  this->spi_sample_counter_unwrap_valid_ = false;
  this->spi_last_raw_sample_counter_ = 0;
  this->spi_sample_counter_epoch_ = 0;
  std::fill(this->spi_last_cross_sample_valid_, this->spi_last_cross_sample_valid_ + 3, false);
  std::fill(this->spi_last_cross_sample_, this->spi_last_cross_sample_ + 3, SpiCrossingPosition{});
  this->spi_line1_period_valid_ = false;
  this->spi_line1_period_samples_ = 0.0f;
  std::fill(this->spi_last_voltage_sample_valid_, this->spi_last_voltage_sample_valid_ + 3, false);
  std::fill(this->spi_last_voltage_difference_, this->spi_last_voltage_difference_ + 3, 0);
  std::fill(this->spi_last_voltage_sample_counter_, this->spi_last_voltage_sample_counter_ + 3, 0);
  std::fill(this->spi_pending_cross_sample_valid_, this->spi_pending_cross_sample_valid_ + 3, false);
  std::fill(this->spi_pending_cross_sample_, this->spi_pending_cross_sample_ + 3, SpiCrossingPosition{});
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
  if (denominator == 0 || numerator == 0 || sum == 0) {
    return 0;
  }
  if (sum > UINT64_MAX / numerator) {
    return UINT16_MAX;
  }
  const uint64_t scaled = (sum * numerator) / denominator;
  const uint32_t root = integer_sqrt_u64(scaled);
  if (root >= UINT16_MAX) {
    return UINT16_MAX;
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
  const uint32_t reference_samples =
      this->hardware_id_ == 3 ? SPI_VUE3_MAIN_SAMPLE_COUNT : SPI_VUE2_MAIN_SAMPLE_COUNT;
  uint32_t samples =
      (reference_samples * interval_ms + (SPI_REFERENCE_WINDOW_MS / 2)) / SPI_REFERENCE_WINDOW_MS;
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

void EmporiaVueComponent::push_spi_fundamental_sample_(const SpiRawScan &scan,
                                                       const SpiRawScan &main_current_scan,
                                                       const SpiRawScan &mux_current_scan) {
  if (this->spi_voltage_sample_ring_count_ != 0 &&
      scan.sample_counter != this->spi_voltage_sample_ring_last_counter_ + 1U) {
    this->spi_voltage_sample_ring_index_ = 0;
    this->spi_voltage_sample_ring_count_ = 0;
  }
  auto &sample = this->spi_voltage_sample_ring_[this->spi_voltage_sample_ring_index_];
  for (uint8_t phase = 0; phase < 3; phase++) {
    sample.voltage[phase] = scan.value[phase * 2];
    sample.main_current[phase] = main_current_scan.value[phase * 2 + 1];
  }
  sample.mux_current[0] = mux_current_scan.value[6];
  sample.mux_current[1] = mux_current_scan.value[7];
  sample.mux_index = scan.mux_index;
  this->spi_voltage_sample_ring_index_ =
      static_cast<uint16_t>((this->spi_voltage_sample_ring_index_ + 1U) % SPI_VOLTAGE_SAMPLE_RING_SIZE);
  if (this->spi_voltage_sample_ring_count_ < SPI_VOLTAGE_SAMPLE_RING_SIZE) {
    this->spi_voltage_sample_ring_count_++;
  }
  this->spi_voltage_sample_ring_last_counter_ = scan.sample_counter;
}

void EmporiaVueComponent::configure_spi_analysis_requirements_() {
  this->spi_current_fundamental_mask_ = 0;
  this->spi_voltage_thd_mask_ = 0;
  this->spi_waveform_analysis_required_ = false;

  for (const auto *phase : this->metering_phases_) {
    if (phase == nullptr || phase->get_input_wire() >= 3) {
      continue;
    }
    if (phase->get_voltage_thd_sensor() != nullptr) {
      this->spi_voltage_thd_mask_ |= 1U << phase->get_input_wire();
    }
    if (phase->get_phase_angle_sensor() != nullptr || phase->get_voltage_thd_sensor() != nullptr) {
      this->spi_waveform_analysis_required_ = true;
    }
  }

  for (const auto *ct_clamp : this->metering_ct_clamps_) {
    if (ct_clamp == nullptr || !ct_clamp->requires_current_fundamental()) {
      continue;
    }
    const uint8_t port = ct_clamp->get_input_port();
    if (port < 19) {
      this->spi_current_fundamental_mask_ |= 1UL << port;
      this->spi_waveform_analysis_required_ = true;
    }
  }
}

float EmporiaVueComponent::spi_crossing_difference_(const SpiCrossingPosition &end,
                                                    const SpiCrossingPosition &start) {
  const float fractional_difference = end.fraction - start.fraction;
  if (end.sample_counter >= start.sample_counter) {
    return static_cast<float>(end.sample_counter - start.sample_counter) + fractional_difference;
  }
  return -static_cast<float>(start.sample_counter - end.sample_counter) + fractional_difference;
}

bool EmporiaVueComponent::accumulate_spi_voltage_cycle_(const SpiCrossingPosition &start_cross_sample,
                                                        const SpiCrossingPosition &end_cross_sample) {
  const float period_samples = spi_crossing_difference_(end_cross_sample, start_cross_sample);
  if (period_samples < 300.0f || period_samples > 700.0f || this->spi_voltage_sample_ring_count_ == 0) {
    return false;
  }

  const uint16_t first_index = static_cast<uint16_t>(
      (this->spi_voltage_sample_ring_index_ + SPI_VOLTAGE_SAMPLE_RING_SIZE - this->spi_voltage_sample_ring_count_) %
      SPI_VOLTAGE_SAMPLE_RING_SIZE);
  const uint64_t first_sample_counter =
      this->spi_voltage_sample_ring_last_counter_ - this->spi_voltage_sample_ring_count_ + 1U;
  const SpiCrossingPosition first_sample{first_sample_counter, 0.0f};
  const SpiCrossingPosition last_sample{this->spi_voltage_sample_ring_last_counter_, 0.0f};
  const float start_from_first = spi_crossing_difference_(start_cross_sample, first_sample);
  const float end_from_last = spi_crossing_difference_(end_cross_sample, last_sample);
  if (start_from_first < -0.5f || end_from_last > 0.5f) {
    return false;
  }

  const float phase_step = SPI_TWO_PI / period_samples;
  const float step_cos = std::cos(phase_step);
  const float step_sin = std::sin(phase_step);
  const float first_from_start = -start_from_first;
  const float first_phase = phase_step * first_from_start;
  const float first_fundamental_cos = std::cos(first_phase);
  const float first_fundamental_sin = std::sin(first_phase);

  auto sample_weight = [first_from_start, period_samples](uint16_t offset) -> float {
    const float sample_from_start = first_from_start + static_cast<float>(offset);
    const float overlap_start = std::max(0.0f, sample_from_start - 0.5f);
    const float overlap_end = std::min(period_samples, sample_from_start + 0.5f);
    return overlap_end - overlap_start;
  };

  // Verify the complete cycle before writing anything to the window accumulator.
  // This lets the large current and THD workspaces be accumulated directly instead
  // of placing almost 2 KiB of temporary arrays on the SPI receive task's stack.
  float cycle_weight = 0.0f;
  for (uint16_t offset = 0; offset < this->spi_voltage_sample_ring_count_; offset++) {
    cycle_weight += std::max(0.0f, sample_weight(offset));
  }
  if (cycle_weight < period_samples * 0.99f) {
    return false;
  }

  auto &acc = this->spi_metering_accumulator_;
  float cycle_i[3]{};
  float cycle_q[3]{};
  float fundamental_cos = first_fundamental_cos;
  float fundamental_sin = first_fundamental_sin;
  for (uint16_t offset = 0; offset < this->spi_voltage_sample_ring_count_; offset++) {
    const uint16_t index = static_cast<uint16_t>((first_index + offset) % SPI_VOLTAGE_SAMPLE_RING_SIZE);
    const auto &sample = this->spi_voltage_sample_ring_[index];
    const float weight = sample_weight(offset);
    if (weight > 0.0f) {
      for (uint8_t phase = 0; phase < 3; phase++) {
        cycle_i[phase] += static_cast<float>(sample.voltage[phase]) * fundamental_cos * weight;
        cycle_q[phase] += static_cast<float>(sample.voltage[phase]) * fundamental_sin * weight;
        if ((this->spi_current_fundamental_mask_ & (1UL << phase)) == 0) {
          continue;
        }
        const int32_t current_difference =
            static_cast<int32_t>(sample.main_current[phase]) - this->spi_adc_offsets_[3 + phase];
        const float oriented_current = -static_cast<float>(current_difference);
        acc.current_fund_i[phase] += oriented_current * fundamental_cos * weight;
        acc.current_fund_q[phase] += oriented_current * fundamental_sin * weight;
        acc.current_fund_weight[phase] += weight;
      }
      if (sample.mux_index < 8) {
        for (uint8_t mux_side = 0; mux_side < 2; mux_side++) {
          const uint8_t internal_index = static_cast<uint8_t>(sample.mux_index * 2 + mux_side);
          const uint8_t clamp_index = static_cast<uint8_t>(3 + internal_index);
          const uint8_t output_port = spi_mux_output_port(this->hardware_id_, internal_index);
          if (output_port >= 19 || (this->spi_current_fundamental_mask_ & (1UL << output_port)) == 0) {
            continue;
          }
          const uint8_t offset_index = static_cast<uint8_t>(6 + internal_index);
          const int32_t current_difference =
              static_cast<int32_t>(sample.mux_current[mux_side]) - this->spi_adc_offsets_[offset_index];
          const float oriented_current = -static_cast<float>(current_difference);
          acc.current_fund_i[clamp_index] += oriented_current * fundamental_cos * weight;
          acc.current_fund_q[clamp_index] += oriented_current * fundamental_sin * weight;
          acc.current_fund_weight[clamp_index] += weight;
        }
      }
    }
    const float next_cos = fundamental_cos * step_cos - fundamental_sin * step_sin;
    fundamental_sin = fundamental_sin * step_cos + fundamental_cos * step_sin;
    fundamental_cos = next_cos;
  }

  for (uint8_t phase = 0; phase < 3; phase++) {
    acc.voltage_fund_i[phase] += cycle_i[phase];
    acc.voltage_fund_q[phase] += cycle_q[phase];
  }

  if (this->spi_voltage_thd_mask_ != 0) {
    // The dedicated metering task has ample measured stack headroom for the
    // complete H2..H40 workspace. Processing all bins together avoids walking
    // the same voltage cycle five times without changing the calculation.
    static constexpr uint8_t HARMONIC_BATCH_SIZE = SPI_VOLTAGE_THD_HARMONIC_COUNT;
    float order_cos = first_fundamental_cos;
    float order_sin = first_fundamental_sin;
    float order_step_cos = step_cos;
    float order_step_sin = step_sin;
    for (uint8_t harmonic_base = 0; harmonic_base < SPI_VOLTAGE_THD_HARMONIC_COUNT;
         harmonic_base = static_cast<uint8_t>(harmonic_base + HARMONIC_BATCH_SIZE)) {
      const uint8_t harmonic_count = std::min<uint8_t>(
          HARMONIC_BATCH_SIZE, static_cast<uint8_t>(SPI_VOLTAGE_THD_HARMONIC_COUNT - harmonic_base));
      float harmonic_i[3][HARMONIC_BATCH_SIZE]{};
      float harmonic_q[3][HARMONIC_BATCH_SIZE]{};
      float harmonic_cos[HARMONIC_BATCH_SIZE]{};
      float harmonic_sin[HARMONIC_BATCH_SIZE]{};
      float harmonic_step_cos[HARMONIC_BATCH_SIZE]{};
      float harmonic_step_sin[HARMONIC_BATCH_SIZE]{};

      // Build the oscillators for this bounded H2..H40 batch recursively.
      for (uint8_t harmonic = 0; harmonic < harmonic_count; harmonic++) {
        const float next_order_cos =
            order_cos * first_fundamental_cos - order_sin * first_fundamental_sin;
        order_sin = order_sin * first_fundamental_cos + order_cos * first_fundamental_sin;
        order_cos = next_order_cos;
        harmonic_cos[harmonic] = order_cos;
        harmonic_sin[harmonic] = order_sin;

        const float next_order_step_cos = order_step_cos * step_cos - order_step_sin * step_sin;
        order_step_sin = order_step_sin * step_cos + order_step_cos * step_sin;
        order_step_cos = next_order_step_cos;
        harmonic_step_cos[harmonic] = order_step_cos;
        harmonic_step_sin[harmonic] = order_step_sin;
      }

      for (uint16_t offset = 0; offset < this->spi_voltage_sample_ring_count_; offset++) {
        const uint16_t index = static_cast<uint16_t>((first_index + offset) % SPI_VOLTAGE_SAMPLE_RING_SIZE);
        const auto &sample = this->spi_voltage_sample_ring_[index];
        const float weight = sample_weight(offset);
        if (weight > 0.0f) {
          for (uint8_t phase = 0; phase < 3; phase++) {
            if ((this->spi_voltage_thd_mask_ & (1U << phase)) == 0) {
              continue;
            }
            const float voltage = static_cast<float>(static_cast<int32_t>(sample.voltage[phase]) -
                                                     this->spi_adc_offsets_[phase]);
            for (uint8_t harmonic = 0; harmonic < harmonic_count; harmonic++) {
              harmonic_i[phase][harmonic] += voltage * harmonic_cos[harmonic] * weight;
              harmonic_q[phase][harmonic] += voltage * harmonic_sin[harmonic] * weight;
            }
          }
        }
        for (uint8_t harmonic = 0; harmonic < harmonic_count; harmonic++) {
          const float next_harmonic_cos =
              harmonic_cos[harmonic] * harmonic_step_cos[harmonic] -
              harmonic_sin[harmonic] * harmonic_step_sin[harmonic];
          harmonic_sin[harmonic] = harmonic_sin[harmonic] * harmonic_step_cos[harmonic] +
                                   harmonic_cos[harmonic] * harmonic_step_sin[harmonic];
          harmonic_cos[harmonic] = next_harmonic_cos;
        }
      }

      for (uint8_t phase = 0; phase < 3; phase++) {
        if ((this->spi_voltage_thd_mask_ & (1U << phase)) == 0) {
          continue;
        }
        acc.voltage_thd_requested[phase] = true;
        for (uint8_t harmonic = 0; harmonic < harmonic_count; harmonic++) {
          const uint8_t output_index = static_cast<uint8_t>(harmonic_base + harmonic);
          acc.voltage_harmonic_i[phase][output_index] += harmonic_i[phase][harmonic];
          acc.voltage_harmonic_q[phase][output_index] += harmonic_q[phase][harmonic];
        }
      }
    }
  }

  acc.voltage_fund_weight += cycle_weight;
  acc.voltage_fund_cycle_count++;
  return true;
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

  if (!this->spi_sample_counter_unwrap_valid_) {
    this->spi_sample_counter_unwrap_valid_ = true;
    this->spi_last_raw_sample_counter_ = sample_counter;
  } else {
    if (sample_counter < this->spi_last_raw_sample_counter_ &&
        (this->spi_last_raw_sample_counter_ - sample_counter) > (UINT32_MAX / 2U)) {
      this->spi_sample_counter_epoch_ += (uint64_t{1} << 32U);
    }
    this->spi_last_raw_sample_counter_ = sample_counter;
  }
  const uint64_t unwrapped_sample_counter = this->spi_sample_counter_epoch_ + sample_counter;

  const uint8_t *payload = frame + SPI_RAW_FRAME_HEADER_SIZE;
  for (uint8_t scan_index = 0; scan_index < SPI_RAW_SCAN_COUNT; scan_index++) {
    const uint8_t current_index = this->spi_raw_scan_ring_index_;
    auto &scan = this->spi_raw_scan_ring_[current_index];
    const uint8_t *scan_data = payload + static_cast<uint16_t>(scan_index) * SPI_RAW_SCAN_SIZE;
    auto read_i16 = [](const uint8_t *data, uint8_t offset) -> int16_t {
      const uint16_t raw = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
      return static_cast<int16_t>(raw);
    };
    for (uint8_t value_index = 0; value_index < 8; value_index++) {
      scan.value[value_index] = read_i16(scan_data, static_cast<uint8_t>(value_index * 2));
    }
    scan.sample_counter = unwrapped_sample_counter + scan_index;
    scan.mux_index = scan_data[16];

    this->process_spi_raw_scan_(current_index);
    auto &acc = this->spi_metering_accumulator_;
    if (this->spi_metering_window_synced_ && acc.line1_period_count != 0) {
      const float period_samples = acc.cycle_count[0] == 0
                                       ? 0.0f
                                       : static_cast<float>(acc.cycle_sum[0] / acc.cycle_count[0]);
      if (acc.target_periods == 0) {
        acc.target_periods = this->spi_metering_target_periods_(period_samples);
      }
      if (acc.line1_period_count >= acc.target_periods) {
        this->finish_spi_metering_window_(sequence, flags);
      }
    }
  }
}

void EmporiaVueComponent::process_spi_raw_scan_(uint8_t current_index) {
  const auto &scan = this->spi_raw_scan_ring_[current_index];
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
  if (this->spi_waveform_analysis_required_) {
    this->push_spi_fundamental_sample_(scan, main_current_scan, mux_current_scan);
  }

  bool sync_window_after_scan = false;

  for (uint8_t phase = 0; phase < 3; phase++) {
    const int32_t voltage_difference = voltage_differences[phase];
    auto interpolated_crossing_sample = [this, phase, voltage_difference, &scan]() -> SpiCrossingPosition {
      if (this->spi_last_voltage_sample_valid_[phase]) {
        const int32_t previous_difference = this->spi_last_voltage_difference_[phase];
        const int32_t delta = voltage_difference - previous_difference;
        if (previous_difference < 0 && delta > 0) {
          const int32_t crossing_numerator = -previous_difference;
          const float fraction = crossing_numerator >= delta
                                     ? 1.0f
                                     : static_cast<float>(crossing_numerator) / static_cast<float>(delta);
          return SpiCrossingPosition{this->spi_last_voltage_sample_counter_[phase], fraction};
        }
      }
      return SpiCrossingPosition{scan.sample_counter, 0.0f};
    };

    uint8_t &cycle_state = this->spi_cycle_state_[phase];
    if (cycle_state == 43) {
      if (voltage_difference < 1) {
        this->spi_pending_cross_sample_valid_[phase] = false;
        cycle_state = 40;
      } else {
        const SpiCrossingPosition crossing_sample = this->spi_pending_cross_sample_valid_[phase]
                                                        ? this->spi_pending_cross_sample_[phase]
                                                        : interpolated_crossing_sample();
        this->spi_pending_cross_sample_valid_[phase] = false;
        if (phase == 0) {
          if (this->spi_last_cross_sample_valid_[0]) {
            const float period = spi_crossing_difference_(crossing_sample, this->spi_last_cross_sample_[0]);
            if (period >= 300.0f && period <= 700.0f) {
              this->spi_line1_period_samples_ = period;
              this->spi_line1_period_valid_ = true;
              if (this->spi_metering_window_synced_) {
                if (this->spi_waveform_analysis_required_) {
                  this->accumulate_spi_voltage_cycle_(this->spi_last_cross_sample_[0], crossing_sample);
                }
                acc.cycle_sum[0] += period;
                acc.cycle_count[0]++;
                acc.line1_period_count++;
                if (acc.line1_period_sample_count < 64) {
                  if (acc.line1_period_sample_count == 0) {
                    acc.line1_period_min = period;
                    acc.line1_period_max = period;
                  } else {
                    acc.line1_period_min = std::min(acc.line1_period_min, period);
                    acc.line1_period_max = std::max(acc.line1_period_max, period);
                  }
                  acc.line1_period_sum += period;
                  acc.line1_period_sample_count++;
                }
              } else {
                sync_window_after_scan = true;
              }
            }
          }
          this->spi_last_cross_sample_[0] = crossing_sample;
          this->spi_last_cross_sample_valid_[0] = true;
        } else if (this->spi_line1_period_valid_ && this->spi_last_cross_sample_valid_[0]) {
          const float period = this->spi_line1_period_samples_;
          float offset = spi_crossing_difference_(crossing_sample, this->spi_last_cross_sample_[0]);
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
    acc.current_peak_abs[phase] =
        std::max(acc.current_peak_abs[phase], static_cast<uint32_t>(std::abs(current_difference)));
    for (uint8_t voltage_phase = 0; voltage_phase < 3; voltage_phase++) {
      acc.raw_power_sum[phase][voltage_phase] -=
          static_cast<int64_t>(current_difference) * voltage_differences[voltage_phase];
    }
  }

  acc.voltage_product_sum[0] +=
      static_cast<int64_t>(voltage_differences[0]) * voltage_differences[1];
  acc.voltage_product_sum[1] +=
      static_cast<int64_t>(voltage_differences[0]) * voltage_differences[2];
  acc.voltage_product_sum[2] +=
      static_cast<int64_t>(voltage_differences[1]) * voltage_differences[2];

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
      acc.current_peak_abs[clamp_index] =
          std::max(acc.current_peak_abs[clamp_index], static_cast<uint32_t>(std::abs(current_difference)));
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
  auto mux_samples_for_port = [this, &acc](uint8_t port) -> uint32_t {
    if (port < 3) {
      return acc.sample_count;
    }
    for (uint8_t internal_index = 0; internal_index < 16; internal_index++) {
      if (spi_mux_output_port(this->hardware_id_, internal_index) == port) {
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
      const uint32_t sample_rate_tenths =
          this->spi_sample_rate_hz_ <= 0.0f ? 0U : static_cast<uint32_t>(this->spi_sample_rate_hz_ * 10.0f + 0.5f);
      ESP_LOGD(TAG,
               "SAMD09 SPI metering window rejected: %s samples=%" PRIu32 "/%" PRIu32
               " periods=%u cycles=%u/%u/%u min_cycles=%" PRIu32 " min_mux=%" PRIu32 "/%" PRIu32
               " sample_rate=%" PRIu32 ".%" PRIu32 "Hz",
               invalid_reason, acc.sample_count, this->spi_metering_target_samples_(),
               static_cast<unsigned>(acc.line1_period_count), static_cast<unsigned>(acc.cycle_count[0]),
               static_cast<unsigned>(acc.cycle_count[1]), static_cast<unsigned>(acc.cycle_count[2]),
               min_cycle_count, logged_min_mux_samples, min_mux_samples, sample_rate_tenths / 10U,
               sample_rate_tenths % 10U);
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
  for (uint8_t phase = 0; phase < 3; phase++) {
    frame.voltage_square_raw[phase] = static_cast<float>(acc.voltage_square_sum[phase]) *
                                      SPI_VOLTAGE_STATISTIC_SCALE_MULTIPLIER /
                                      static_cast<float>(acc.sample_count);
  }
  for (uint8_t pair = 0; pair < 3; pair++) {
    frame.voltage_product_raw[pair] = static_cast<float>(acc.voltage_product_sum[pair]) *
                                      SPI_VOLTAGE_STATISTIC_SCALE_MULTIPLIER /
                                      static_cast<float>(acc.sample_count);
  }
  frame.voltage_statistics_valid = true;

  auto robust_line1_period = [&acc]() -> float {
    if (acc.line1_period_sample_count == 0) {
      return 0.0f;
    }
    float sum = acc.line1_period_sum;
    uint8_t count = acc.line1_period_sample_count;
    if (acc.line1_period_sample_count >= 5) {
      sum -= acc.line1_period_min + acc.line1_period_max;
      count = static_cast<uint8_t>(count - 2);
    }
    return sum / static_cast<float>(count);
  };

  float line1_period_samples = robust_line1_period();
  if (line1_period_samples <= 0.0f && acc.cycle_count[0] != 0) {
    line1_period_samples = static_cast<float>(acc.cycle_sum[0] / acc.cycle_count[0]);
  }
  if (line1_period_samples > 0.0f) {
    frame.phases[0].phase_angle_degrees = 0.0f;
    frame.phases[0].cycle_count_raw = normalize_cycle_count(line1_period_samples);
    if (this->spi_sample_rate_hz_ > 1.0f) {
      const float frequency = this->spi_sample_rate_hz_ / line1_period_samples;
      for (auto &phase : frame.phases) {
        phase.frequency_hz = frequency;
      }
    }
  }

  float voltage_fund_angle[3]{};
  bool voltage_fund_angle_valid[3]{};
  const bool fundamental_window_valid = acc.voltage_fund_cycle_count >= min_cycle_count;
  if (fundamental_window_valid) {
    for (uint8_t phase = 0; phase < 3; phase++) {
      const float i = acc.voltage_fund_i[phase];
      const float q = acc.voltage_fund_q[phase];
      const float magnitude = std::sqrt(i * i + q * q);
      const float amplitude =
          acc.voltage_fund_weight <= 0.0f ? 0.0f : (2.0f * magnitude / acc.voltage_fund_weight);
      if (amplitude >= SPI_MIN_VOLTAGE_FUNDAMENTAL_AMPLITUDE) {
        frame.phases[phase].voltage_fundamental_i_raw =
            acc.voltage_fund_i[phase] * SPI_FUNDAMENTAL_RMS_COMPONENT_SCALE / acc.voltage_fund_weight;
        frame.phases[phase].voltage_fundamental_q_raw =
            acc.voltage_fund_q[phase] * SPI_FUNDAMENTAL_RMS_COMPONENT_SCALE / acc.voltage_fund_weight;
        frame.phases[phase].voltage_fundamental_valid = true;
        if (acc.voltage_thd_requested[phase]) {
          // All bins share the same cycle weight, so their raw phasor magnitude ratio is the THD ratio.
          double harmonic_magnitude_squared = 0.0;
          for (uint8_t index = 0; index < SPI_VOLTAGE_THD_HARMONIC_COUNT; index++) {
            const double harmonic_i = acc.voltage_harmonic_i[phase][index];
            const double harmonic_q = acc.voltage_harmonic_q[phase][index];
            harmonic_magnitude_squared += harmonic_i * harmonic_i + harmonic_q * harmonic_q;
          }
          const double fundamental_magnitude_squared = static_cast<double>(i) * i + static_cast<double>(q) * q;
          if (fundamental_magnitude_squared > 0.0) {
            frame.phases[phase].voltage_thd_percent =
                static_cast<float>(100.0 * std::sqrt(harmonic_magnitude_squared / fundamental_magnitude_squared));
            frame.phases[phase].voltage_thd_valid = std::isfinite(frame.phases[phase].voltage_thd_percent);
          }
        }
        voltage_fund_angle[phase] = std::atan2(q, i);
        voltage_fund_angle_valid[phase] = true;
      }
    }
  }
  if (voltage_fund_angle_valid[0] && line1_period_samples > 0.0f) {
    for (uint8_t phase = 1; phase < 3; phase++) {
      if (!voltage_fund_angle_valid[phase]) {
        continue;
      }
      float angle = (voltage_fund_angle[phase] - voltage_fund_angle[0]) * 360.0f / SPI_TWO_PI;
      while (angle < 0.0f) {
        angle += 360.0f;
      }
      while (angle >= 360.0f) {
        angle -= 360.0f;
      }
      frame.phases[phase].phase_angle_degrees = angle;
      frame.phases[phase].cycle_count_raw = normalize_cycle_count(angle * line1_period_samples / 360.0f);
    }
  }

  auto set_current_fundamental = [&acc, fundamental_window_valid](MeteringClamp &clamp, uint8_t source_index) {
    const float weight = acc.current_fund_weight[source_index];
    if (!fundamental_window_valid || weight < 8.0f) {
      return;
    }
    clamp.current_fundamental_i_raw =
        acc.current_fund_i[source_index] * SPI_FUNDAMENTAL_RMS_COMPONENT_SCALE / weight;
    clamp.current_fundamental_q_raw =
        acc.current_fund_q[source_index] * SPI_FUNDAMENTAL_RMS_COMPONENT_SCALE / weight;
    clamp.current_fundamental_valid = true;
  };
  auto set_current_peak = [this, &acc](MeteringClamp &clamp, uint8_t source_index) {
    if (this->spi_offset_warmup_windows_ < SPI_ADC_OFFSET_STARTUP_WINDOWS) {
      return;
    }
    const uint64_t scaled =
        static_cast<uint64_t>(acc.current_peak_abs[source_index]) * SPI_CURRENT_PEAK_SCALE_MULTIPLIER;
    clamp.current_peak_raw =
        scaled >= UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(scaled);
    clamp.current_peak_valid = true;
  };

  for (uint8_t phase = 0; phase < 3; phase++) {
    frame.phases[phase].voltage_raw =
        scale_spi_rms_(static_cast<uint64_t>(acc.voltage_square_sum[phase]), SPI_MAIN_RMS_SCALE_NUMERATOR,
                       acc.sample_count);
    frame.clamps[phase].current_raw =
        scale_spi_rms_(static_cast<uint64_t>(acc.current_square_sum[phase]), SPI_CURRENT_RMS_SCALE_NUMERATOR,
                       acc.sample_count);
    set_current_peak(frame.clamps[phase], phase);
    if ((this->spi_current_fundamental_mask_ & (1UL << phase)) != 0) {
      set_current_fundamental(frame.clamps[phase], phase);
    }
    for (uint8_t voltage_phase = 0; voltage_phase < 3; voltage_phase++) {
      frame.clamps[phase].power_raw_by_phase[voltage_phase] =
          scale_spi_power_(acc.raw_power_sum[phase][voltage_phase], SPI_POWER_SCALE_MULTIPLIER, acc.sample_count);
    }
    if (phase == 0 || !std::isnan(frame.phases[phase].phase_angle_degrees)) {
      continue;
    }
    if (acc.cycle_count[phase] != 0) {
      const float cycle_samples = static_cast<float>(acc.cycle_sum[phase] / acc.cycle_count[phase]);
      frame.phases[phase].cycle_count_raw = normalize_cycle_count(cycle_samples);
      if (phase > 0 && line1_period_samples > 0.0f) {
        frame.phases[phase].phase_angle_degrees = cycle_samples * 360.0f / line1_period_samples;
      }
    }
  }

  for (uint8_t internal_index = 0; internal_index < 16; internal_index++) {
    const uint8_t output_port = spi_mux_output_port(this->hardware_id_, internal_index);
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
    set_current_peak(frame.clamps[output_port], source_index);
    if ((this->spi_current_fundamental_mask_ & (1UL << output_port)) != 0) {
      set_current_fundamental(frame.clamps[output_port], source_index);
    }
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

  // Read the cross-task timestamp before millis(). If the receive task updates
  // it afterwards, the confirmation below suppresses a stale timeout instead
  // of underflowing an unsigned subtraction and reporting a 1 ms stall.
  const uint32_t last_valid_frame_ms = this->spi_rx_last_valid_frame_ms_;
  const uint32_t now = millis();
  if (!this->spi_rx_recover_requested_ && last_valid_frame_ms != 0 &&
      now - last_valid_frame_ms >= SPI_RX_VALID_FRAME_TIMEOUT_MS &&
      this->spi_rx_last_valid_frame_ms_ == last_valid_frame_ms) {
    this->spi_rx_stall_recovery_ = true;
    this->spi_rx_recover_requested_ = true;
  }
  if (this->spi_rx_recover_requested_) {
    if (this->spi_rx_stall_recovery_ && this->spi_rx_last_valid_frame_ms_ != last_valid_frame_ms) {
      this->spi_rx_stall_recovery_ = false;
      this->spi_rx_recover_requested_ = false;
    } else {
      this->restart_spi_receiver_();
      return;
    }
  }

  const bool status_changed =
      !this->spi_rx_logged_status_valid_ || this->spi_rx_sync_errors_ != this->spi_rx_logged_sync_errors_ ||
      this->spi_rx_crc_errors_ != this->spi_rx_logged_crc_errors_ ||
      this->spi_rx_queue_errors_ != this->spi_rx_logged_queue_errors_ ||
      this->spi_processing_overruns_ != this->spi_rx_logged_processing_overruns_ ||
      this->spi_rx_dma_errors_ != this->spi_rx_logged_dma_errors_ ||
      this->spi_rx_samd_overruns_ != this->spi_rx_logged_samd_overruns_ ||
      this->spi_rx_frame_gaps_ != this->spi_rx_logged_frame_gaps_ ||
      this->spi_rx_recoveries_ != this->spi_rx_logged_recoveries_ ||
      this->spi_rx_last_flags_ != this->spi_rx_logged_flags_;
  if (status_changed && now - this->spi_rx_last_log_ms_ >= METERING_STATUS_LOG_INTERVAL_MS) {
    this->spi_rx_last_log_ms_ = now;
    const UBaseType_t processing_pending = this->spi_processing_ready_queue_ == nullptr
                                               ? 0
                                               : uxQueueMessagesWaiting(this->spi_processing_ready_queue_);
    ESP_LOGD(TAG,
             "SAMD09 SPI rx status: sync_errors=%" PRIu32 " crc_errors=%" PRIu32 " queue_errors=%" PRIu32
             " processing_overruns=%" PRIu32 " processing_pending=%u dma_errors=%" PRIu32
             " samd_overruns=%" PRIu32 " seq_gaps=%" PRIu32 " recoveries=%" PRIu32
             " inflight=%" PRIu16 " flags=0x%04" PRIx32 " sample_counter=%" PRIu32,
             this->spi_rx_sync_errors_, this->spi_rx_crc_errors_, this->spi_rx_queue_errors_,
             this->spi_processing_overruns_, static_cast<unsigned>(processing_pending), this->spi_rx_dma_errors_,
             this->spi_rx_samd_overruns_, this->spi_rx_frame_gaps_, this->spi_rx_recoveries_, this->spi_rx_inflight_,
             this->spi_rx_last_flags_, this->spi_rx_last_sample_counter_);
    this->spi_rx_logged_status_valid_ = true;
    this->spi_rx_logged_sync_errors_ = this->spi_rx_sync_errors_;
    this->spi_rx_logged_crc_errors_ = this->spi_rx_crc_errors_;
    this->spi_rx_logged_queue_errors_ = this->spi_rx_queue_errors_;
    this->spi_rx_logged_processing_overruns_ = this->spi_processing_overruns_;
    this->spi_rx_logged_dma_errors_ = this->spi_rx_dma_errors_;
    this->spi_rx_logged_samd_overruns_ = this->spi_rx_samd_overruns_;
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
