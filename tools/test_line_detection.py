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
  for (bool exported : {false,true}) for (uint8_t line : {1,2,3}) {
    const float direction=exported ? -1.f : 1.f;
    Fixture rising(exported); rising.window(0,0,0,line);
    rising.hold(direction*230,0,1,4,line);
    assert(rising.ct.selected==line && rising.ct.assignments==1);
    Fixture falling(exported); falling.window(direction*230,0,1,line);
    falling.hold(0,0,0,4,line); assert(falling.ct.selected==line);
    Fixture moderate(exported); moderate.window(0,0,0,line);
    moderate.hold(direction*200,direction*150,250.f/230.f,14,line);
    // PF=.8 alone is not a strong enough endpoint under the default guard.
    assert(moderate.ct.selected==0 && moderate.ct.sensor.state=="ambiguous change");
    Fixture reactive_change(exported); reactive_change.window(direction*10,-direction*20,0.90f,line);
    reactive_change.hold(direction*19.3f,-direction*109.7f,0.75f,14,line);
    assert(reactive_change.ct.selected==0 && reactive_change.ct.sensor.state=="ambiguous change");
    Fixture reactive_start(exported); reactive_start.window(0,0,0,line);
    reactive_start.hold(direction*19.3f,-direction*109.7f,0.75f,14,line);
    assert(reactive_start.ct.selected==0 && reactive_start.ct.sensor.state=="ambiguous change");
    for (int degrees=-90;degrees<=90;degrees+=5) {
      const float angle=degrees*3.14159265358979323846f/180.f;
      for (auto transport : {MeteringTransport::I2C,MeteringTransport::SPI}) {
        Fixture sweep(exported); sweep.frame.transport=transport;
        sweep.window(0,0,0,line);
        sweep.hold(direction*230*std::cos(angle),direction*230*std::sin(angle),1,4,line);
        assert(sweep.ct.selected==0 || sweep.ct.selected==line);
        if (degrees>=-10 && degrees<=10) assert(sweep.ct.selected==line);
      }
    }
    Fixture reactive(exported); reactive.window(direction*5,-direction*400,400.f/230.f,line);
    reactive.hold(direction*230,0,1,4,line); assert(reactive.ct.selected==line);
    Fixture noisy(exported); noisy.window(0,0,0.50f,line);
    for(int i=0;i<4;i++) noisy.window(direction*50,0,i%2 ? 0.65f : 0.60f,line);
    assert(noisy.ct.selected==line);
    Fixture small_rms_change(exported); small_rms_change.window(0,0,0.58f,line);
    small_rms_change.hold(direction*50,0,0.60f,4,line);
    assert(small_rms_change.ct.selected==line);
    Fixture small_rms_falling(exported); small_rms_falling.window(direction*50,0,0.60f,line);
    small_rms_falling.hold(0,0,0.58f,4,line);
    assert(small_rms_falling.ct.selected==line);
    Fixture variable_power(exported); variable_power.window(0,0,0,line);
    for(int p : {50,75,100,125}) variable_power.window(direction*p,0,p/230.f,line);
    assert(variable_power.ct.selected==line);
    Fixture reactive_standby(exported);
    reactive_standby.hold(0,direction*230,1,14,line);
    assert(reactive_standby.ct.selected==0);
    Fixture rotated(exported); rotated.window(direction*230,0,1,line);
    rotated.hold(0,direction*230,1,14,line);
    assert(rotated.ct.selected==0);
  }
  Fixture idle; idle.hold(0,0,0,20); assert(idle.ct.selected==0);
  Fixture unchanged; unchanged.hold(230,0,1,20); assert(unchanged.ct.selected==0);
  Fixture reversal; reversal.window(5,-400,400.f/230.f);
  reversal.hold(230,0,1); assert(reversal.ct.selected==1);
  Fixture reverse_stop; reverse_stop.window(230,0,1);
  reverse_stop.hold(5,-400,400.f/230.f); assert(reverse_stop.ct.selected==1);
  Fixture ramp; ramp.window(0,0,0);
  for(int p=20;p<=100;p+=20) ramp.window(p,0,p/230.f);
  ramp.hold(100,0,100.f/230.f); assert(ramp.ct.selected==1);
  Fixture rotation; rotation.window(230,0,1);
  rotation.hold(0,230,1,6); assert(rotation.ct.selected==0);
  Fixture boundary; boundary.window(0,0,0);
  boundary.hold(115,199.186f,1,6); assert(boundary.ct.selected==0);
  Fixture contradictory; contradictory.window(230,0,1,1);
  contradictory.hold(460,0,2,6,2); assert(contradictory.ct.selected==0);
  Fixture diagnostic; diagnostic.ct.auto_active=false; diagnostic.window(0,0,0);
  diagnostic.hold(230,0,1); assert(diagnostic.ct.selected==0 && diagnostic.ct.sensor.state=="L1");
  Fixture standalone; standalone.ct.auto_active=false;
  standalone.frame.clamps[3].current_fundamental_valid=false;
  for(auto &phase : standalone.frame.phases) phase.voltage_fundamental_valid=false;
  standalone.window(0,0,0.58f,3); standalone.hold(50,0,0.60f,4,3);
  assert(standalone.ct.selected==0 && standalone.ct.sensor.state=="L3");
  Fixture auto_only; auto_only.ct.diagnostic_enabled=false;
  auto_only.frame.clamps[3].current_fundamental_valid=false;
  auto_only.window(0,0,0.58f,3); auto_only.hold(50,0,0.60f,4,3);
  assert(auto_only.ct.selected==3);
  Fixture changing_line; changing_line.window(0,0,0);
  for(int i=0;i<14;i++) changing_line.window(50,0,0.25f,1+i%2);
  assert(changing_line.ct.selected==0);
  Fixture i2c; i2c.frame.transport=MeteringTransport::I2C; i2c.ct.correction=70;
  i2c.window(0,0,0); i2c.hold(230,0,1); assert(i2c.ct.selected==1);
  Fixture i2c_small; i2c_small.frame.transport=MeteringTransport::I2C;
  i2c_small.window(0,0,0.58f,3); i2c_small.hold(50,0,0.60f,4,3);
  assert(i2c_small.ct.selected==3);
  Fixture two_lines; two_lines.ct.candidates.pop_back();
  two_lines.window(0,0,0,2); two_lines.hold(230,0,1,4,2); assert(two_lines.ct.selected==2);
  for(bool exported : {false,true}) for(uint8_t line : {1,2}) for(float angle : {178.f,180.f,182.f}) {
    Fixture split(exported); split.ct.candidates.pop_back();
    split.frame.phases[1].phase_angle_degrees=angle;
    split.window(0,0,0,line);
    split.hold(exported ? -50.f : 50.f,0,0.25f,4,line);
    assert(split.ct.selected==line);
  }
  Fixture calibrated; calibrated.ct.correction=-70.f; calibrated.window(0,0,0);
  calibrated.hold(230.f*std::cos(70.f*3.14159265f/180.f),230.f*std::sin(70.f*3.14159265f/180.f),1);
  assert(calibrated.ct.selected==1); // Candidate calibration works without an assigned line.
  Fixture missing; missing.ct.correction=5; missing.frame.phases[1].voltage_fundamental_valid=false;
  missing.window(0,0,0); missing.hold(230,0,1,6); assert(missing.ct.selected==0);
  assert(!missing.ct.automatic.has_reference());
  Fixture transient; transient.window(0,0,0); transient.window(230,0,1);
  transient.hold(0,0,0,6); assert(transient.ct.selected==0);
  Fixture expired; expired.window(0,0,0); expired.hold(0,0,0,13);
  assert(simulated_ms-expired.ct.automatic.get_reference_start_ms()<20000);
  Fixture wrapped; wrapped.window(0,0,0); simulated_ms=UINT32_MAX-5000;
  wrapped.ct.automatic.set_reference({0,0,0},0,simulated_ms);
  wrapped.ct.diagnostic.set_reference({0,0,0},0,simulated_ms);
  wrapped.ct.automatic.set_window_start_ms(simulated_ms);
  wrapped.ct.diagnostic.set_window_start_ms(simulated_ms);
  wrapped.hold(230,0,1); assert(wrapped.ct.selected==1);
  std::puts("PASS line detection: import/export, I2C/SPI, all lines, phase sweep, reactive rejection, start/stop, reactive standby, ramps, noisy RMS, small RMS delta, ambiguity, standalone detection");
}
"""
    compile_run(PRELUDE + state + frames + mocks + body + tests)


if __name__ == "__main__":
    test_line_detection()
