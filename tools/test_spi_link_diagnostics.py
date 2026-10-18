#!/usr/bin/env python3
"""Exercise production SPI link diagnostics with mocked clocks, sensors and SWD."""

import importlib.util

from test_esphome_metering import HEADER, PRELUDE, ROOT, SPI, compile_run, component_fields, function


def main():
    publish = function(SPI, "EmporiaVueComponent::publish_spi_diagnostics_")
    # Hardware memory/stack diagnostics are unrelated to these link counters.
    publish = publish[:publish.index("#ifdef USE_ESP32")] + "}\n"
    summary = function(SPI, "EmporiaVueComponent::log_spi_summary_")
    fault = function(SPI, "EmporiaVueComponent::check_samd_hard_fault_")
    setup = function(SPI, "EmporiaVueComponent::setup_spi_receiver_")
    start = setup.index("  if (reset_statistics) {")
    end, depth = setup.index("{", start) + 1, 1
    while depth:
        depth += (setup[end] == "{") - (setup[end] == "}")
        end += 1
    reset = "void EmporiaVueComponent::reset_for_test(bool reset_statistics) {\n" + setup[start:end] + "\n}\n"
    bodies = [publish, summary, fault, reset]
    declarations = [body[:body.index("{")].replace("EmporiaVueComponent::", "").rstrip() + ";"
                    for body in bodies]
    mocks = r"""
namespace sensor {
struct Sensor { float state{NAN}; unsigned count{0};
  void publish_state(float value) {state=value; ++count;}
};
}
static constexpr const char *TAG = "test";
"""
    extra = r"""
  struct SpiFrameValidationResult {};
  static constexpr uint32_t SAMD_BOOT_DIAG_BASE=0x20000000U, SAMD_BOOT_DIAG_MAGIC=0x45565344U;
  static constexpr uint32_t SAMD_BOOT_STAGE_HARDFAULT=255, DHCSR=0xe000edf0U, DHCSR_DBGKEY=0xa05f0000U;
  std::array<uint32_t,7> record{};
  unsigned reads=0, releases=0, resumes=0, debug_off=0;
  int failed_read=-1;
  bool attach_ok=true;
  void prepare_pins_() {}
  void begin_swd_session_() {}
  void finish_swd_session_() {}
  bool swd_initialize_(uint32_t *id) {*id=1; return attach_ok;}
  bool power_up_debug_() {return true;}
  bool verify_mem_ap_() {return true;}
  bool halt_core_() {return true;}
  bool mem_read32_(uint32_t addr,uint32_t *value) {
    assert(addr==SAMD_BOOT_DIAG_BASE+4*reads && reads<record.size());
    unsigned index=reads++;
    if(int(index)==failed_read) return false;
    *value=record[index]; return true;
  }
  bool resume_core_() {++resumes; return true;}
  bool mem_write32_(uint32_t addr,uint32_t value) {
    assert(addr==DHCSR && value==DHCSR_DBGKEY); ++debug_off; return true;
  }
  void release_pins_() {++releases;}
  void publish_spi_voltage_diagnostics_() {}
"""
    tests = r"""
int main() {
  EmporiaVueComponent c;
  sensor::Sensor gaps, faults, rate;
  c.diag_frame_gaps_sensor_=&gaps;
  c.diag_hard_faults_sensor_=&faults;
  c.diag_valid_frame_rate_sensor_=&rate;
  c.spi_rx_frame_gaps_=8; c.spi_rx_hard_faults_=2;
  simulated_ms=1000; c.spi_rx_valid_frames_=100; c.publish_spi_diagnostics_();
  assert(gaps.state==8 && faults.state==2 && rate.count==0);
  simulated_ms=11000; c.spi_rx_valid_frames_=3600; c.publish_spi_diagnostics_();
  assert(rate.state==350.0f);
  // Natural clock and counter wrap still use unsigned deltas.
  c.spi_diag_last_rate_ms_=UINT32_MAX-4999;
  c.spi_diag_last_valid_frames_=UINT32_MAX-3499;
  simulated_ms=5000; c.spi_rx_valid_frames_=0; c.publish_spi_diagnostics_();
  assert(rate.state==350.0f);
  c.spi_summary_last_ms_=1000; c.spi_summary_last_valid_frames_=100;
  c.spi_rx_valid_frames_=21100; c.log_spi_summary_(61000);
  assert(c.spi_summary_last_ms_==61000 && c.spi_summary_last_valid_frames_==21100);
  // Soft recovery retains the cumulative counters and matching rate baselines.
  c.reset_for_test(false);
  assert(c.spi_rx_valid_frames_==21100 && c.spi_summary_last_valid_frames_==21100);
  assert(c.spi_diag_last_rate_ms_==5000 && c.spi_rx_hard_faults_==2);
  // A new statistics epoch must not publish a huge false rate after SAMD flash.
  c.reset_for_test(true);
  assert(c.spi_rx_valid_frames_==0 && c.spi_rx_hard_faults_==0);
  assert(c.spi_summary_last_ms_==0 && c.spi_summary_last_valid_frames_==0);
  assert(c.spi_diag_last_rate_ms_==0 && c.spi_diag_last_valid_frames_==0);
  unsigned published=rate.count;
  simulated_ms=20000; c.publish_spi_diagnostics_(); assert(rate.count==published);
  simulated_ms=30000; c.spi_rx_valid_frames_=3500; c.publish_spi_diagnostics_();
  assert(rate.state==350.0f);
  for(unsigned kind=0;kind<4;++kind) {
    EmporiaVueComponent d;
    d.record={d.SAMD_BOOT_DIAG_MAGIC,255,0xfffffff9U,0x20000f88U,0,0x2000002cU,0x11cbU};
    if(kind==1) d.record[0]=0; // Foreign/stock firmware: no diagnostic record.
    if(kind==2) d.record[1]=28; // Managed firmware without a hard fault.
    if(kind==3) d.failed_read=3;
    d.check_samd_hard_fault_();
    assert(d.spi_rx_hard_faults_==(kind==0 ? 1U : 0U));
    assert(d.reads==(kind==3 ? 4U : 7U) && d.releases==1);
    assert(d.resumes==(kind==3 ? 0U : 1U) && d.debug_off==d.resumes);
  }
  EmporiaVueComponent absent; absent.attach_ok=false;
  absent.check_samd_hard_fault_();
  assert(absent.reads==0 && absent.releases==1 && absent.spi_rx_hard_faults_==0);
  std::puts("PASS SPI link diagnostics: rates, wrap, full reset/soft recovery, bounded SWD and fault records");
}
"""
    compile_run(PRELUDE + mocks + "\nclass EmporiaVueComponent {public:\n" + extra +
                component_fields(*bodies) + "\n" + "\n".join(declarations) +
                "\n};\n" + "\n".join(bodies) + tests)
    spec = importlib.util.spec_from_file_location("ev_link_test", ROOT / "components/emporiavue/__init__.py")
    component = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(component)
    keys = {"diag_frame_gaps", "diag_valid_frame_rate", "diag_hard_faults"}
    for hardware in ("vue2", "vue3"):
        spi = component._apply_diagnostics_defaults({"hardware": hardware, "mode": "spi", "diagnostics_interval": "10s"})
        i2c = component._apply_diagnostics_defaults({"hardware": hardware, "mode": "i2c", "diagnostics_interval": "10s"})
        assert keys <= spi.keys() and not keys.intersection(i2c)
    print("PASS SPI link diagnostic defaults: enabled for both Vue versions, not I2C")


if __name__ == "__main__":
    main()
