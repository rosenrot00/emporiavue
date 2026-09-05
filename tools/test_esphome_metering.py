#!/usr/bin/env python3
"""Host regression tests; run with an ESPHome Python environment and a C++ compiler.

Compile the production metering methods, mocking only ESPHome/FreeRTOS I/O.
No device, network access, or generated repository firmware headers are needed.
"""

from pathlib import Path
import importlib.util
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "components/emporiavue/emporiavue.h").read_text()
SPI = (ROOT / "components/emporiavue/spi_transport.cpp").read_text()
METERING = (ROOT / "components/emporiavue/metering.cpp").read_text()


def function(source, name):
    match = re.search(r"^(?:[\w:]+\s+)+" + re.escape(name) + r"\([^;]*?\)(?: const)? \{", source, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def compile_run(program):
    with tempfile.TemporaryDirectory(prefix="emporiavue-metering-") as folder:
        binary = str(Path(folder) / "test")
        subprocess.run(
            ["c++", "-std=c++17", "-O1", "-fsanitize=undefined", "-fno-sanitize-recover=all",
             "-x", "c++", "-", "-o", binary], input=program, text=True, check=True,
        )
        subprocess.run([binary], check=True)


PRELUDE = r"""
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <limits>
#include <vector>
#include <cstdio>
#include <cassert>
#include <string>
#include <functional>
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGV(...) ((void)0)
uint32_t simulated_ms = 0;
uint32_t millis() { return simulated_ms; }
enum class MeteringTransport : uint8_t { UNKNOWN, SPI, I2C };
"""


def test_spi_windows():
    methods = ["reset_spi_metering_state_", "sanitize_spi_adc_offset_", "scale_spi_rms_", "scale_spi_power_",
               "spi_metering_target_samples_", "spi_metering_target_periods_", "spi_crossing_difference_",
               "decode_spi_raw_frame_", "process_spi_raw_scan_", "finish_spi_metering_window_"]
    bodies = [function(SPI, "EmporiaVueComponent::" + name) for name in methods]
    declarations = [body[:body.index("{")].replace("EmporiaVueComponent::", "").rstrip() + ";" for body in bodies]
    queue_marker = "#ifdef USE_ESP32\n  if (this->spi_metering_queue_"
    assert queue_marker in bodies[-1]
    bodies[-1] = bodies[-1].replace(queue_marker, "output.push_back(frame);\ncounts.push_back(acc.sample_count);\n" + queue_marker)
    structs = HEADER[HEADER.index("  struct MeteringPhase {"):HEADER.index("  enum MemSize")]
    fields = HEADER[HEADER.index("  SpiMeteringAccumulator spi_metering_accumulator_"):HEADER.index("  volatile uint16_t spi_rx_inflight_")]
    constants = HEADER[HEADER.index("static constexpr float VUE2_STOCK_CYCLE"):HEADER.index("class MeteringPhaseConfig;")]
    helpers = SPI[SPI.index("static constexpr uint8_t SPI_VUE2_RAW_FRAME_VERSION"):SPI.index("void EmporiaVueComponent::publish_spi_diagnostics_")]
    mocks = r"""
struct MeteringPhaseConfig { uint8_t input; uint8_t get_input_wire() const {return input;} };
struct MeteringCTClampConfig {
  uint8_t get_input_port() const {return 0;}
  const MeteringPhaseConfig* get_phase() const {return nullptr;}
  const MeteringPhaseConfig* get_line_pair_phase_b() const {return nullptr;}
  bool is_line_pair() const {return false;}
};
struct VirtualLine {
  const MeteringPhaseConfig* get_line_a() const {return nullptr;}
  const MeteringPhaseConfig* get_line_b() const {return nullptr;}
};
"""
    extra = r"""
  uint16_t hardware_id_{2};
  uint32_t metering_interval_ms_{220};
  float spi_sample_rate_hz_{0};
  uint32_t spi_last_frame_samples_{0}, spi_invalid_window_last_log_ms_{0};
  std::vector<MeteringPhaseConfig*> metering_phases_;
  std::vector<MeteringCTClampConfig*> metering_ct_clamps_;
  std::vector<VirtualLine*> metering_virtual_lines_;
  std::vector<MeteringFrame> output;
  std::vector<uint32_t> counts;
  void push_spi_fundamental_sample_(const SpiRawScan&,const SpiRawScan&,const SpiRawScan&) {}
  bool accumulate_spi_voltage_cycle_(const SpiCrossingPosition&,const SpiCrossingPosition&) {return false;}
"""
    tests = r"""
int main() {
  for (uint16_t hw : {2,3}) for (uint32_t interval : {220U,513U,UINT32_MAX}) {
    EmporiaVueComponent c;
    c.hardware_id_ = hw;
    c.metering_interval_ms_ = interval;
    c.spi_main_current_delay_ = c.spi_mux_current_delay_ = 0;
    MeteringPhaseConfig phases[3]{{0},{1},{2}};
    for (auto &p: phases) c.metering_phases_.push_back(&p);
    for (auto &mask : c.spi_power_voltage_mask_) mask = 7;
    const float rate = 16000000.f/(hw == 2 ? 632.f : 816.f);
    uint32_t counter = 0, sequence = 0;
    auto feed = [&](float seconds, bool drop_reference) {
      for (unsigned f = 0; f < static_cast<unsigned>(seconds*rate/56); ++f) {
        uint8_t frame[1024]{};
        for (unsigned s=0;s<56;++s) {
          auto data = frame + 12 + s*18;
          for (unsigned p=0;p<3;++p) {
            double angle = (counter+s)*2.*3.141592653589793*50/rate-p*2.*3.141592653589793/3.;
            int16_t v = drop_reference && p==0 ? 0 : std::lround(1000.*std::sin(angle));
            int16_t i = std::lround(-200.*std::sin(angle));
            data[p*4]=v&255; data[p*4+1]=(v>>8)&255;
            data[p*4+2]=i&255; data[p*4+3]=(i>>8)&255;
          }
          data[16]=(counter+s)%8;
        }
        simulated_ms = counter*1000./rate;
        c.spi_sample_rate_hz_=rate;
        c.decode_spi_raw_frame_(frame,sequence++,0,counter);
        assert(c.spi_metering_accumulator_.sample_count < SPI_MAX_WINDOW_SAMPLES);
        counter+=56;
      }
    };
    feed(1.6,false);
    assert(!c.output.empty());
    if (interval == UINT32_MAX) assert(c.spi_metering_target_samples_() == SPI_MAX_METERING_SAMPLES);
    const auto normal = c.output.back().phases[1].voltage_raw;
    assert(std::abs(int(normal)-7070)<5);
    for (int repeat=0;repeat<3;++repeat) {
      auto before = c.output.size();
      feed(4.0,true); // Raw packets continue normally; only reference crossings disappear.
      assert(c.output.size()==before);
      feed(1.6,false);
      assert(c.output.size()>before);
      for (auto n=before;n<c.output.size();++n) {
        assert(c.output[n].valid && c.counts[n]<SPI_MAX_WINDOW_SAMPLES);
        assert(std::abs(int(c.output[n].phases[1].voltage_raw)-int(normal))<10);
      }
    }
    std::printf("PASS Vue %u SPI: bounded windows and clean recovery (interval=%u)\n",hw,interval);
  }
}
"""
    compile_run(PRELUDE + mocks + constants + helpers + "\nclass EmporiaVueComponent {public:\n" +
                structs + fields + extra + "\n".join(declarations) + "\n};\n" + "\n".join(bodies) + tests)


def test_energy_gaps():
    methods = ["setup", "dump_config", "publish_state_and_save", "schedule_midnight_reset_", "process_metering_state_"]
    bodies = "\n".join(function(METERING, "MeteringDailyEnergy::" + name) for name in methods)
    energy_class = HEADER[HEADER.index("enum class MeteringEnergyMethod"):HEADER.index("class MeteringPowerFilters")]
    energy_class = energy_class.replace("protected:", "public:").replace("time::", "mock_time::")
    mocks = r"""
#define LOG_SENSOR(...) ((void)0)
struct Application { uint32_t get_loop_component_start_time() {return simulated_ms;} } App;
struct ESPPreferenceObject {
  float value{123}; unsigned saves{0}, loads{0};
  void load(float *out) {++loads; *out=value;}
  void save(float *in) {++saves; value=*in;}
};
struct Component {
  virtual void setup() {}
  virtual void dump_config() {}
  uint32_t timeout_ms{0}; std::function<void()> timeout;
  void set_timeout(uint32_t id, uint32_t ms, std::function<void()> fn) {assert(id==1); timeout_ms=ms; timeout=fn;}
};
namespace sensor {struct Sensor {
  float state{NAN}; unsigned callbacks{0}; std::function<void(float)> callback;
  void publish_state(float value) {state=value;}
  template<typename T> ESPPreferenceObject make_entity_preference() {return {};}
  void add_on_state_callback(std::function<void(float)> fn) {++callbacks; callback=fn;}
};}
namespace mock_time {
struct ClockState {
  bool valid{false}; unsigned day_of_year{1}, hour{0}, minute{0}, second{0};
  bool is_valid() const {return valid;}
};
struct RealTimeClock {
  ClockState state; std::function<void()> callback;
  ClockState now() {return state;}
  void add_on_time_sync_callback(std::function<void()> fn) {callback=fn;}
};
}
"""
    tests = r"""
int main() {
  for (auto method : {MeteringEnergyMethod::LEFT,MeteringEnergyMethod::RIGHT,MeteringEnergyMethod::TRAPEZOID}) {
    MeteringDailyEnergy e;
    mock_time::RealTimeClock clock; sensor::Sensor parent;
    e.set_method(method); e.set_parent(&parent); e.set_time(&clock);
    e.set_energy_scale(1.0f);
    simulated_ms=10000;
    e.setup();
    assert(parent.callbacks==1 && e.pref_.loads==1 && e.state==123 && e.timeout_ms==0);
    parent.callback(3600);
    assert(e.total_energy_==123); // Do not count startup time, including right/trapezoid modes.
    simulated_ms+=1000;
    e.process_metering_state_(3600);
    assert(std::abs(e.total_energy_-124)<0.001);
    // No timeout callback during a stalled loop: the energy guard must still work.
    simulated_ms+=600000;
    e.process_metering_state_(3600);
    assert(std::abs(e.total_energy_-124)<0.001);
    simulated_ms+=1000;
    e.process_metering_state_(3600);
    assert(std::abs(e.total_energy_-125)<0.001);
    for (float invalid : {NAN,INFINITY,-INFINITY}) {
      e.process_metering_state_(invalid);
      simulated_ms+=500;
      e.process_metering_state_(3600);
      assert(std::abs(e.total_energy_-125)<0.001);
    }
    // millis() wrap is a normal short interval, not a 49-day integration gap.
    simulated_ms=UINT32_MAX-500;
    e.process_metering_state_(3600);
    simulated_ms+=1000;
    e.process_metering_state_(3600);
    assert(std::abs(e.total_energy_-126)<0.001);
    simulated_ms+=1000;
    e.process_metering_state_(-3600);
    const float change = method==MeteringEnergyMethod::LEFT ? 1 : method==MeteringEnergyMethod::RIGHT ? -1 : 0;
    assert(std::abs(e.total_energy_-(126+change))<0.001);
    assert(e.pref_.value==e.total_energy_);
    clock.state.valid=true; clock.callback();
    assert(e.timeout_ms==81000000 && e.total_energy_>0); // First clock sync retains restored total.
    clock.state.hour=23; clock.state.minute=59; clock.state.second=59; clock.callback();
    assert(e.timeout_ms==2000 && e.total_energy_>0);
    clock.state.day_of_year=2; clock.state.hour=0; clock.state.minute=0; clock.state.second=1;
    e.timeout();
    assert(e.state==0 && e.pref_.value==0); // Scheduled midnight reset, same Wh persistence.
    e.set_restore(false); auto saves=e.pref_.saves; e.publish_state_and_save(7);
    assert(e.state==7 && e.pref_.saves==saves);
  }
  MeteringDailyEnergy units;
  units.publish_state_and_save(1000);
  assert(units.state==1 && units.total_energy_==1000 && units.pref_.value==1000);
  units.set_energy_scale(0.000001f); units.publish_state_and_save(1000000);
  assert(units.state==1 && units.total_energy_==1000000 && units.pref_.value==1000000);
  std::puts("PASS daily energy: restore, midnight, startup, gaps, invalid samples, methods, signed power and millis wrap");
}
"""
    compile_run(PRELUDE + mocks + energy_class + bodies + tests)


def test_energy_filters():
    spec = importlib.util.spec_from_file_location("ev_test", ROOT / "components/emporiavue/__init__.py")
    component = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(component)
    cases = [
        ({}, 0.001, 0),
        ({"filters": [{"throttle": "5s"}]}, 0.001, 0),
        ({"filters": [{"multiply": 0.001}, {"throttle": "5s"}]}, 0.001, 0),
        ({"filters": [{"throttle": "5s"}, {"multiply": 0.001}]}, 0.001, 0),
        ({"filters": [{"multiply": 1.1}]}, 0.0011, 1),
        ({"unit_of_measurement": "Wh"}, 1.0, 0),
        ({"unit_of_measurement": "MWh"}, 0.000001, 0),
    ]
    # Reproduce the original local-filter override of a global .001 conversion.
    parent = component._apply_filter_default_to_sensor(
        {"energy": {"filters": [{"throttle": "5s"}]}}, "energy",
        {"energy": [{"multiply": 0.001}]}, "test",
    )
    cases.append((parent["energy"], 0.001, 0))
    for index, (config, expected, count) in enumerate(cases):
        validated = component._validate_energy_sensor({"name": f"Energy {index}", **config})
        filters = validated["filters"]
        multipliers = [f["multiply"] for f in filters if "multiply" in f]
        result = component.ENERGY_UNIT_SCALES[validated["unit_of_measurement"]]
        for multiplier in multipliers:
            result *= multiplier
        assert len(multipliers) == count and abs(result - expected) < 1e-12
    print("PASS energy units: bare, local/global throttle, legacy conversion, Wh/kWh/MWh and calibration")


def test_metering_timeout():
    methods = ["submit_metering_frame_", "check_metering_timeout_", "invalidate_metering_",
               "apply_power_direction_", "publish_power_outputs_"]
    bodies = [function(METERING, "EmporiaVueComponent::" + name) for name in methods]
    declarations = [body[:body.index("{")].replace("EmporiaVueComponent::", "").rstrip() + ";" for body in bodies]
    # Every measurement sensor touched by the real invalidation path gets a
    # separate mock. Counters verify that only transient analysis is reset.
    getters = sorted(set(re.findall(r"->(get_\w+_sensor)\(\)", bodies[2])))
    getters.remove("get_line_detection_sensor")
    getter_methods = "\n".join(f"sensor::Sensor *{name}() {{return &sensors[{index}];}}" for index, name in enumerate(getters))
    power_class = HEADER[HEADER.index("class MeteringPowerOutput {"):HEADER.index("class MeteringDemandTracker")]
    peak_class = HEADER[HEADER.index("class MeteringPeakTracker {"):HEADER.index("class EmporiaVueComponent")]
    peak_methods = "\n".join(function(METERING, "MeteringPeakTracker::" + name) for name in
                             ["finish_window_", "loop", "invalidate_window", "add_sample"])
    mocks = r"""
namespace sensor {struct Sensor {
  float state{1}; unsigned count{0};
  void publish_state(float value) {state=value; ++count;}
};}
struct Text {std::string state; void publish_state(const char *value) {state=value;}};
struct Detection {unsigned resets{0}; void reset_all() {++resets;}};
"""
    node = r"""
struct Node {
  sensor::Sensor sensors[32], raw, visible;
  std::vector<MeteringPowerOutput> outputs{{0,&raw,&visible}};
  Text text;
  Detection detection, assignment;
  float energy{123}, maximum{456};
  int saved_line{2};
  unsigned power_gaps{0}, current_gaps{0}, peak_resets{0};
  const std::vector<MeteringPowerOutput>& get_power_outputs() {return outputs;}
  Text* get_line_detection_sensor() {return &text;}
  Detection& get_line_detection_state() {return detection;}
  Detection& get_auto_line_detection_state() {return assignment;}
  void add_power_demand_sample(float value, uint32_t) {assert(std::isnan(value)); ++power_gaps;}
  void add_current_demand_sample(float value, uint32_t) {assert(std::isnan(value)); ++current_gaps;}
  void invalidate_peak(uint32_t) {++peak_resets;}
""" + getter_methods + "\n};\n"
    component = r"""
struct MeteringFrame {
  bool valid{true}; uint32_t timestamp_ms{0}, sequence{0};
  MeteringTransport transport{MeteringTransport::SPI}; uint8_t quality_flags{0};
};
class EmporiaVueComponent {public:
  uint32_t metering_timeout_ms_{2000}, last_metering_frame_ms_{0}, last_unavailable_publish_ms_{0};
  bool metering_data_stale_{true}, last_metering_sequence_valid_{false};
  uint32_t last_metering_sequence_{0}, i2c_missing_readings_window_{0};
  MeteringTransport last_metering_transport_{MeteringTransport::UNKNOWN};
  std::vector<Node*> metering_phases_, metering_virtual_lines_, metering_ct_clamps_, metering_groups_;
  unsigned published{0};
  void publish_metering_frame_(const MeteringFrame&) {++published;}
""" + "\n".join(declarations) + "\n};\n"
    tests = r"""
int main() {
  for (auto transport : {MeteringTransport::SPI,MeteringTransport::I2C}) {
    EmporiaVueComponent c; Node ct, group, phase, virtual_line;
    c.metering_ct_clamps_={&ct}; c.metering_groups_={&group};
    c.metering_phases_={&phase}; c.metering_virtual_lines_={&virtual_line};
    MeteringFrame frame; frame.transport=transport;
    simulated_ms=1000; c.check_metering_timeout_(simulated_ms);
    assert(ct.raw.count==0); // Not started yet.
    frame.timestamp_ms=simulated_ms; frame.sequence=1; c.submit_metering_frame_(frame);
    simulated_ms=2000; c.check_metering_timeout_(simulated_ms); assert(ct.raw.count==0);
    simulated_ms=3001; c.check_metering_timeout_(simulated_ms);
    assert(std::isnan(ct.raw.state) && std::isnan(group.raw.state));
    assert(ct.power_gaps==1 && ct.current_gaps==1 && group.power_gaps==1);
    assert(ct.detection.resets==1 && ct.assignment.resets==1 && ct.text.state=="unavailable");
    assert(ct.energy==123 && ct.maximum==456 && ct.saved_line==2);
    const auto count=ct.raw.count;
    simulated_ms=3500; c.check_metering_timeout_(simulated_ms); assert(ct.raw.count==count);
    simulated_ms=4001; c.check_metering_timeout_(simulated_ms); assert(ct.raw.count==count+1);
    assert(ct.power_gaps==1 && ct.detection.resets==1); // No repeated analysis resets.
    c.submit_metering_frame_(frame); assert(c.published==1); // Old queued frame rejected.
    frame.timestamp_ms=simulated_ms; frame.sequence=4; c.submit_metering_frame_(frame);
    assert(c.published==2 && !c.metering_data_stale_);
    assert(c.i2c_missing_readings_window_==(transport==MeteringTransport::I2C ? 2U : 0U));
    simulated_ms=8000; frame.timestamp_ms=simulated_ms; frame.sequence=5;
    c.submit_metering_frame_(frame); // Main loop stalled: reset even without a timeout call.
    assert(ct.power_gaps==2 && c.published==3);
    frame.valid=false; c.submit_metering_frame_(frame); assert(c.published==3);
    frame.valid=true; simulated_ms=UINT32_MAX-1000; frame.timestamp_ms=simulated_ms;
    c.submit_metering_frame_(frame);
    auto resets=ct.power_gaps;
    simulated_ms+=1500; c.check_metering_timeout_(simulated_ms); assert(ct.power_gaps==resets);
    frame.timestamp_ms=simulated_ms; c.submit_metering_frame_(frame); assert(ct.power_gaps==resets);
  }
  MeteringPeakTracker peak; sensor::Sensor current, crest;
  peak.set_current_peak_sensor(&current); peak.set_current_crest_factor_sensor(&crest);
  peak.add_sample(10,20,0); peak.invalidate_window(2000); peak.loop(5000);
  assert(std::isnan(current.state) && std::isnan(crest.state));
  peak.add_sample(1,2,6000); peak.loop(11000);
  assert(current.state==1 && crest.state==2); // Old peak must not survive the gap.
  std::puts("PASS I2C/SPI freshness: stale queue, stalled loop, retries, saved state, peaks and millis wrap");
}
"""
    compile_run(PRELUDE + mocks + power_class + peak_class + node + component + "\n".join(bodies) + peak_methods + tests)


if __name__ == "__main__":
    test_spi_windows()
    test_energy_gaps()
    test_energy_filters()
    test_metering_timeout()
