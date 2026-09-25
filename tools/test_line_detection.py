#!/usr/bin/env python3
"""Exercise the production line-detection state machine with synthetic loads."""
from test_esphome_metering import HEADER, METERING, PRELUDE, compile_run, function


def test_line_detection():
    state = HEADER[HEADER.index("class MeteringLineDetectionState {"):HEADER.index("class MeteringCTClampConfig {")]
    frames = HEADER[HEADER.index("  struct MeteringPhase {"):HEADER.index("  struct SpiRawScan {")]
    body = function(METERING, "EmporiaVueComponent::update_line_detection_")
    mocks = r"""
#include <cstdarg>
std::string str_sprintf(const char *format, ...) {
  char buffer[256]; va_list args; va_start(args,format);
  vsnprintf(buffer,sizeof(buffer),format,args); va_end(args); return buffer;
}
namespace text_sensor { struct TextSensor {
  std::string state;
  bool has_state() const {return !state.empty();}
  void publish_state(const std::string &value) {state=value;}
}; }
struct MeteringPhaseConfig {
  uint8_t wire; float calibration{0.022f};
  uint8_t get_input_wire() const {return wire;}
  float get_calibration() const {return calibration;}
};
struct MeteringCTClampConfig {
  struct Candidate {MeteringPhaseConfig *phase; uint8_t line;};
  std::vector<Candidate> candidates;
  text_sensor::TextSensor sensor;
  MeteringLineDetectionState diagnostic, automatic;
  bool auto_active{true}, diagnostic_enabled{true}, export_direction{false};
  uint8_t selected{0}; unsigned assignments{0}; float correction{0};
  uint8_t get_input_port() const {return 3;}
  const std::vector<Candidate>& get_line_detection_candidates() const {return candidates;}
  float get_current_gain() const {return 1.f;}
  float get_current_phase_correction() const {return correction;}
  text_sensor::TextSensor *get_line_detection_sensor() {return diagnostic_enabled ? &sensor : nullptr;}
  bool is_auto_line_detection_active() const {return auto_active;}
  bool is_auto_line_detection_export() const {return export_direction;}
  bool is_line_detection_export() const {return export_direction;}
  float get_auto_line_detection_power_min() const {return 30.f;}
  float get_line_detection_power_min() const {return 30.f;}
  const std::string &get_line_detection_name() const {static std::string name="test"; return name;}
  MeteringLineDetectionState &get_line_detection_state() {return diagnostic;}
  MeteringLineDetectionState &get_auto_line_detection_state() {return automatic;}
  void complete_auto_line_detection(uint8_t line) {selected=line; ++assignments; auto_active=false;}
};
class EmporiaVueComponent {public:
  uint32_t line_detection_update_interval_ms_{10000};
  float line_detection_confidence_ratio_{1.5f};
  void update_line_detection_(const MeteringFrame &,MeteringCTClampConfig *);
};
struct Fixture {
  EmporiaVueComponent component; MeteringCTClampConfig ct; MeteringPhaseConfig phases[3]{{0},{1},{2}};
  MeteringFrame frame;
  Fixture(bool exported=false) {
    simulated_ms=100;
    for (int p=0;p<3;p++) {
      ct.candidates.push_back({&phases[p],uint8_t(p+1)});
      frame.phases[p].phase_angle_degrees=p*120.f;
      const float a=p*2.f*3.14159265358979323846f/3.f;
      frame.phases[p].voltage_fundamental_i_raw=230.f*std::cos(a)/0.022f;
      frame.phases[p].voltage_fundamental_q_raw=230.f*std::sin(a)/0.022f;
      frame.phases[p].voltage_fundamental_valid=true;
    }
    frame.clamps[3].current_fundamental_valid=true;
    ct.export_direction=exported; frame.transport=MeteringTransport::SPI;
    frame.clamps[3].power_phase_valid_mask=7;
  }
  // P/Q refer to the real physical line. CT voltage products on other lines
  // follow from the same current vector; RMS is independently supplied.
  void window(float p,float q,float current,uint8_t real_line=1) {
    for (int line=0;line<3;line++) {
      const float a=(frame.phases[line].phase_angle_degrees-frame.phases[real_line-1].phase_angle_degrees)*3.14159265358979323846f/180.f;
      frame.clamps[3].power_raw_by_phase[line]=std::lround((p*std::cos(a)+q*std::sin(a))*1000.f);
    }
    frame.clamps[3].current_raw=std::lround(current*170496.f/775.f);
    const float a=(real_line-1)*2.f*3.14159265358979323846f/3.f;
    frame.clamps[3].current_fundamental_i_raw=(p*std::cos(a)-q*std::sin(a))/230.f*170496.f/775.f;
    frame.clamps[3].current_fundamental_q_raw=(p*std::sin(a)+q*std::cos(a))/230.f*170496.f/775.f;
    simulated_ms+=1; component.update_line_detection_(frame,&ct);
    simulated_ms+=10000; component.update_line_detection_(frame,&ct);
  }
  void hold(float p,float q,float current,int windows=4,uint8_t line=1) {
    for(int i=0;i<windows;i++) window(p,q,current,line);
  }
};
"""
    tests = r"""
int main() {
  // Regression coverage for the pre-September-25 detector, not the reverted
  // endpoint-alignment heuristic. The original limitations are unchanged.
  for (auto transport : {MeteringTransport::I2C,MeteringTransport::SPI})
  for (bool exported : {false,true}) for (uint8_t line : {1,2,3}) {
    const float direction=exported ? -1.f : 1.f;
    Fixture rising(exported); rising.frame.transport=transport;
    rising.window(0,0,0,line);
    rising.window(direction*230,0,1,line); assert(rising.ct.selected==0);
    rising.window(direction*230,0,1,line);
    assert(rising.ct.selected==0 && rising.ct.sensor.state=="L"+std::to_string(line)+" weak");
    rising.window(direction*230,0,1,line);
    assert(rising.ct.selected==line && rising.ct.assignments==1);
    Fixture falling(exported); falling.frame.transport=transport;
    falling.window(direction*230,0,1,line);
    falling.hold(0,0,0,4,line); assert(falling.ct.selected==line);
    for(float q : {-150.f,150.f}) {
      Fixture moderate(exported); moderate.frame.transport=transport;
      moderate.window(0,0,0,line);
      moderate.hold(direction*200,direction*q,250.f/230.f,4,line);
      assert(moderate.ct.selected==line); // PF=.8: negative alternatives do not veto.
    }
    Fixture ratio(exported); ratio.frame.transport=transport;
    ratio.window(0,0,0,line);
    ratio.hold(direction*150,direction*210,1.2f,6,line);
    assert(ratio.ct.selected==0); // Two positive candidates, ratio below 1.5.
    Fixture diagnostic(exported); diagnostic.frame.transport=transport;
    diagnostic.ct.auto_active=false;
    diagnostic.frame.clamps[3].current_fundamental_valid=false;
    for(auto &phase : diagnostic.frame.phases) phase.voltage_fundamental_valid=false;
    diagnostic.window(0,0,0,line); diagnostic.hold(direction*230,0,1,4,line);
    assert(diagnostic.ct.selected==0 && diagnostic.ct.sensor.state=="L"+std::to_string(line));
  }
  Fixture idle; idle.hold(0,0,0,20); assert(idle.ct.selected==0);
  Fixture unchanged; unchanged.hold(230,0,1,20);
  assert(unchanged.ct.selected==0 && unchanged.ct.sensor.state=="waiting for change");
  Fixture rotation; rotation.window(230,0,1);
  rotation.hold(0,230,1,6); assert(rotation.ct.selected==0);
  Fixture transient; transient.window(0,0,0); transient.window(230,0,1);
  transient.hold(0,0,0,6); assert(transient.ct.selected==0);
  Fixture slow_ramp; slow_ramp.window(0,0,0);
  for(int p=20;p<=100;p+=20) slow_ramp.window(p,0,p/230.f);
  slow_ramp.hold(100,0,100.f/230.f);
  assert(slow_ramp.ct.selected==0); // Original rolling reference, deliberately restored.
  for(bool exported : {false,true}) for(uint8_t line : {1,2}) {
    Fixture split(exported); split.ct.candidates.pop_back();
    split.frame.phases[1].phase_angle_degrees=180.f;
    split.window(0,0,0,line);
    split.hold(exported ? -50.f : 50.f,0,0.25f,4,line);
    assert(split.ct.selected==line);
  }
  std::puts("PASS restored line detection: import/export, I2C/SPI, all lines, positive runner-up ratio, PF=.8, start/stop, diagnostic-only, original transition behavior");
}
"""
    compile_run(PRELUDE + state + frames + mocks + body + tests)


if __name__ == "__main__":
    test_line_detection()
