#!/usr/bin/env python3
"""Raw SPI scans through the real waveform/math/journal/entity publication path.

Hardware/RTOS and ESPHome entity I/O are host adapters. Date clipping/persistence
have separate production tests; their energy endpoint is an observable adapter
here. No phasors, physical measurements, configuration classes or publication
methods are replaced. Synthetic delays follow the configured firmware contract,
not a claim of independently measured board timing.
"""

from test_esphome_metering import HEADER, METERING, SPI, PRELUDE, compile_run, function
from test_spi_metering_journal import declaration


def test_spi_entities():
    spi_names = ["reset_spi_metering_state_", "sanitize_spi_adc_offset_", "scale_spi_rms_", "scale_spi_power_",
                 "spi_metering_target_samples_", "spi_metering_target_periods_", "spi_crossing_difference_",
                 "push_spi_fundamental_sample_", "accumulate_spi_voltage_cycle_", "decode_spi_raw_frame_",
                 "process_spi_raw_scan_", "finish_spi_metering_window_", "publish_queued_spi_metering_"]
    math_names = ["calculate_ct_fundamental_phasors_", "calculate_ct_physical_power_", "calculate_ct_power_",
                  "calculate_phase_voltage_", "calculate_line_to_line_voltage_", "calculate_ct_voltage_",
                  "calculate_ct_current_", "calculate_ct_apparent_power_", "calculate_ct_measurement_",
                  "calculate_ct_fundamental_analysis_", "publish_metering_frame_", "submit_metering_frame_",
                  "invalidate_metering_", "apply_power_direction_", "publish_power_outputs_",
                  "calculate_group_power_", "prepare_ct_power_analysis_"]
    bodies = [function(SPI, "EmporiaVueComponent::" + name) for name in spi_names]
    bodies += [function(METERING, "EmporiaVueComponent::" + name) for name in math_names]
    structs = HEADER[HEADER.index("  struct MeteringPhase {"):HEADER.index("  enum MemSize")].replace("protected:", "public:")
    fields = HEADER[HEADER.index("  SpiMeteringAccumulator spi_metering_accumulator_"):
                    HEADER.index("  volatile uint16_t spi_rx_inflight_")]
    constants = HEADER[HEADER.index("static constexpr float VUE2_STOCK_CYCLE"):
                       HEADER.index("class MeteringPhaseConfig;")]
    helpers = SPI[SPI.index("static constexpr uint8_t SPI_VUE2_RAW_FRAME_VERSION"):
                  SPI.index("void EmporiaVueComponent::publish_spi_diagnostics_")]
    classes = (
        HEADER[HEADER.index("class MeteringPowerFilters {"):HEADER.index("class EmporiaVueComponent :")]
        + HEADER[HEADER.index("class MeteringPhaseConfig {"):HEADER.index("class MeteringCalibrationNumber :")]
        + HEADER[HEADER.index("class MeteringLineDetectionState {"):HEADER.index("class EmporiaVueBackupFirmwareButton :")]
    ).replace("protected:", "public:")
    trackers = "\n".join(function(METERING, owner + "::" + name) for owner, methods in (
        ("MeteringDemandTracker", ["setup", "loop", "reset_window_", "day_key_",
                                   "publish_and_save_maximum_", "reset_daily_maximum_", "check_day_",
                                   "finish_bucket_", "add_sample", "add_interval_sample"]),
        ("MeteringPeakTracker", ["finish_window_", "loop", "invalidate_window", "add_sample"]),
        ("MeteringAnalysisTracker", ["add_sample"]),
    ) for name in methods)
    mocks = r"""
#include <deque>
#define USE_ESP32
namespace entity_test {
namespace sensor { struct Sensor {
  float state{NAN}; std::vector<float> published;
  void publish_state(float value) {state=value; published.push_back(value);}
}; }
namespace text_sensor { struct TextSensor {
  std::string state; void publish_state(const char *value) {state=value;}
}; }
struct ESPTime {unsigned year{2026},day_of_year{284}; bool is_valid() const {return true;}};
namespace time {struct RealTimeClock {ESPTime now() const {return {};}};}
struct ESPPreferenceObject {
  template<class T> bool load(T*) {return false;}
  template<class T> bool save(const T*) {return true;}
};
struct Preferences {
  template<class T> ESPPreferenceObject make_preference(uint32_t) {return {};}
} preferences;
auto global_preferences=&preferences;
class MeteringCalibrationNumber;
class MeteringCurrentGainNumber;
class MeteringCurrentPhaseNumber;
class MeteringLineSelect;
struct MeteringDailyEnergy {
  double total{0}; unsigned publications{0};
  double interval_energy_wh(float power,double duration,uint32_t,uint32_t *day=nullptr) const {
    if(day) *day=2026284; return double(power)*duration/3600000.0;
  }
  void add_integral_wh(double value,uint32_t day=0) {
    assert(day==0 || day==2026284); total+=value; ++publications;
  }
};
using UBaseType_t=unsigned;
static constexpr int pdTRUE=1,pdFALSE=0;
struct HostQueue {size_t capacity,element_size; std::deque<std::vector<uint8_t>> records;};
using QueueHandle_t=HostQueue*;
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) {return q->records.size();}
int xQueueSend(QueueHandle_t q,const void *record,unsigned) {
  if(q->records.size()==q->capacity) return pdFALSE;
  const auto *bytes=static_cast<const uint8_t*>(record);
  q->records.emplace_back(bytes,bytes+q->element_size); return pdTRUE;
}
int xQueueReceive(QueueHandle_t q,void *record,unsigned) {
  if(q->records.empty()) return pdFALSE;
  std::memcpy(record,q->records.front().data(),q->element_size);
  q->records.pop_front(); return pdTRUE;
}
"""
    extra = r"""
  CallbackManager<void(const MeteringFrame &)> frame_callbacks_;
  uint16_t hardware_id_{2}; uint32_t metering_interval_ms_{220};
  float spi_sample_rate_hz_{0},minimum_apparent_power_{5},minimum_fundamental_current_{0.02f};
  uint32_t spi_last_frame_samples_{0},spi_invalid_window_last_log_ms_{0};
  uint32_t metering_timeout_ms_{2000},last_metering_frame_ms_{0},last_unavailable_publish_ms_{0};
  bool metering_data_stale_{true},last_metering_sequence_valid_{false};
  uint32_t last_metering_sequence_{0},last_metering_acquisition_epoch_{0},i2c_missing_readings_window_{0};
  uint32_t metering_calculation_generation_{0};
  MeteringTransport last_metering_transport_{MeteringTransport::UNKNOWN};
  std::vector<MeteringPhaseConfig*> metering_phases_;
  std::vector<MeteringVirtualLineConfig*> metering_virtual_lines_;
  std::vector<MeteringCTClampConfig*> metering_ct_clamps_;
  std::vector<MeteringGroupConfig*> metering_groups_;
  QueueHandle_t spi_metering_queue_{nullptr};
  void update_line_detection_(const MeteringFrame&,MeteringCTClampConfig*) {}
"""
    tests = r"""
constexpr double PI=3.14159265358979323846;
using Frame=EmporiaVueComponent::MeteringFrame;
void near(double actual,double expected,double absolute,double relative=0) {
  if (!(std::fabs(actual-expected)<=absolute+std::fabs(expected)*relative)) {
    std::fprintf(stderr,"got %.9g expected %.9g (tolerance %.9g)\n",
                 actual,expected,absolute+std::fabs(expected)*relative);
    assert(false);
  }
}
struct Fixture {
  EmporiaVueComponent c;
  HostQueue queue{32,sizeof(Frame),{}};
  MeteringPhaseConfig phases[3];
  MeteringCTClampConfig clamps[19];
  MeteringGroupConfig group;
  sensor::Sensor current[19],fundamental[19],reactive[19],pf1[19],angle[19],thd[19];
  sensor::Sensor power[19],apparent[19],pf[19],voltage[3],frequency[3],voltage_thd[3],group_power;
  sensor::Sensor imported[19],exported[19],demand[19],current_demand[19],maximum[19],peak[19],crest[19];
  MeteringDailyEnergy energy[19],group_energy;
  MeteringDailyEnergy import_energy[19],export_energy[19];
  time::RealTimeClock clock;
  unsigned filter_calls[19]{};
  uint32_t counter{0},sequence{0};
  float rate,hz,lag;
  bool harmonic;
  double orientation;
  Fixture(uint16_t hw,float frequency,bool harmonics,bool reversed=false,bool inverted=false) :
      rate(16000000.f/(hw==2 ? 632.f : 816.f)),hz(frequency),
      lag(harmonics ? 35.f : -30.f),harmonic(harmonics),orientation((reversed ? -1. : 1.)*(inverted ? -1. : 1.)) {
    c.hardware_id_=hw; c.spi_metering_queue_=&queue; c.spi_sample_rate_hz_=rate;
    c.spi_main_current_delay_=hw==2 ? 2 : 1; c.spi_mux_current_delay_=hw==2 ? 4 : 2;
    c.spi_waveform_analysis_required_=true; c.spi_current_fundamental_mask_=(1UL<<19)-1;
    c.spi_voltage_sample_ring_.reset(new EmporiaVueComponent::SpiFundamentalSample[c.SPI_VOLTAGE_SAMPLE_RING_SIZE]);
    c.spi_voltage_thd_mask_=7;
    c.spi_current_peak_mask_=(1UL<<19)-1;
    for(auto &mask:c.spi_power_voltage_mask_) mask=7;
    for(uint8_t p=0;p<3;++p) {
      phases[p].set_input_wire(p); phases[p].set_calibration(.022f);
      phases[p].set_voltage_sensor(&voltage[p]); phases[p].set_frequency_sensor(&this->frequency[p]);
      phases[p].set_voltage_thd_sensor(&voltage_thd[p]); c.metering_phases_.push_back(&phases[p]);
    }
    for(uint8_t port=0;port<19;++port) {
      auto &ct=clamps[port]; ct.set_input_port(port); ct.set_phase(&phases[port%3]);
      ct.set_polarity_inverted(inverted);
      ct.set_current_sensor(&current[port]); ct.set_apparent_power_sensor(&apparent[port]);
      ct.set_power_factor_sensor(&pf[port]); ct.set_fundamental_current_sensor(&fundamental[port]);
      ct.set_fundamental_reactive_power_sensor(&reactive[port]);
      ct.set_fundamental_power_factor_sensor(&pf1[port]); ct.set_displacement_angle_sensor(&angle[port]);
      ct.set_current_thd_sensor(&thd[port]); ct.set_analysis_interval(0);
      ct.add_power_lambda_filter([this,port](float value){++filter_calls[port];return value*(port==4 ? 2.f : 1.f);});
      ct.add_power_output(0,nullptr,&power[port],&energy[port]); c.metering_ct_clamps_.push_back(&ct);
      ct.add_power_output(1,nullptr,&imported[port],&import_energy[port]);
      ct.add_power_output(2,nullptr,&exported[port],&export_energy[port]);
      ct.set_demand_interval(5000); ct.set_power_demand_sensor(&demand[port]);
      ct.set_current_demand_sensor(&current_demand[port]);
      ct.set_maximum_power_demand_sensor(&maximum[port]); ct.set_power_demand_time(&clock);
      ct.set_power_demand_restore(false); ct.setup_demand();
      simulated_ms=0; ct.loop_demand(); // Establish the day before queued acquisition starts.
      ct.set_peak_interval(500); ct.set_current_peak_sensor(&peak[port]);
      ct.set_current_crest_factor_sensor(&crest[port]);
    }
    // Exercise production gain/phase correction and filtered output semantics.
    clamps[4].set_current_gain(1.1f); clamps[4].set_current_phase_correction(2.5f);
    clamps[4].set_analysis_interval(600); // Actual P/Q aggregation before publishing Q/PF/angle.
    raw_orientation=reversed ? -1. : 1.;
    for(uint8_t port:{3,4,5}) group.add_ct_clamp_term(&clamps[port],1);
    group.add_power_output(0,nullptr,&group_power,&group_energy); c.metering_groups_.push_back(&group);
  }
  double raw_orientation{1};
  double phase_at(uint32_t n,uint8_t p) const {return n*2.*PI*hz/rate-p*2.*PI/3.;}
  double amplitude(uint8_t port) const {return port<3 ? 200.+port*10. : 80.+port*3.;}
  double current_at(uint32_t n,uint8_t port) const {
    const double a=phase_at(n,port%3)-lag*PI/180.;
    return -raw_orientation*amplitude(port)*(std::sin(a)+(harmonic ? .2*std::sin(3*a) : 0.));
  }
  double expected_i1(uint8_t port) const {
    const double scalar=port<3 ? 775./42624. : 775./170496.;
    return amplitude(port)*10.*scalar/std::sqrt(2.)*(port==4 ? 1.1 : 1.);
  }
  double expected_power(uint8_t port) const {
    const double theta=lag*PI/180.;
    const double corrected=(lag+(port==4 ? 2.5 : 0.))*PI/180.;
    return orientation*155.563491861*expected_i1(port)*(std::cos(corrected)+(harmonic ? .01*std::cos(3*theta) : 0.))
             *(port==4 ? 2. : 1.);
  }
  void feed(float seconds) {
    const uint32_t frames=seconds*rate/56;
    for(uint32_t f=0;f<frames;++f) {
      uint8_t frame[1024]{};
      for(uint8_t s=0;s<56;++s) {
        uint8_t *data=frame+12+s*18;
        auto put=[&](uint8_t slot,int value) {
          const uint16_t raw=static_cast<uint16_t>(static_cast<int16_t>(value));
          data[slot*2]=raw&255; data[slot*2+1]=raw>>8;
        };
        for(uint8_t p=0;p<3;++p) {
          const double a=phase_at(counter+s,p);
          put(p*2,std::lround(100.+p*17.+1000.*(std::sin(a)+(harmonic ? .05*std::sin(3*a) : 0.))));
          put(p*2+1,std::lround(100.+p*7.+current_at(counter+s+c.spi_main_current_delay_,p)));
        }
        const uint8_t future_mux=(counter+s+c.spi_mux_current_delay_)%8;
        for(uint8_t side=0;side<2;++side) {
          const uint8_t internal=future_mux*2+side,port=spi_mux_output_port(c.hardware_id_,internal);
          put(6+side,std::lround(230.+internal*5.+current_at(counter+s+c.spi_mux_current_delay_,port)));
        }
        data[16]=(counter+s)%8;
      }
      simulated_ms=counter*1000./rate; c.spi_sample_rate_hz_=rate;
      c.decode_spi_raw_frame_(frame,sequence++,0,counter); counter+=56;
    }
  }
  void check(unsigned drains) {
    assert(queue.records.size()>=5 && c.spi_metering_window_drops_==0);
    const unsigned windows=queue.records.size(); double duration=0,exact_energy[19]{};
    for(const auto &bytes:queue.records) {
      Frame frame; std::memcpy(&frame,bytes.data(),sizeof(frame));
      assert(frame.valid && frame.transport==MeteringTransport::SPI); duration+=frame.duration_ms;
      for(uint8_t port=0;port<19;++port) {
        float physical=0;
        assert(c.calculate_ct_physical_power_(frame,&clamps[port],&physical));
        exact_energy[port]+=double(physical)*(port==4 ? 2. : 1.)*frame.duration_ms/3600000.;
      }
    }
    double previous_energy[19]; unsigned previous_filter_calls[19];
    for(uint8_t port=0;port<19;++port) {
      previous_energy[port]=energy[port].total; previous_filter_calls[port]=filter_calls[port];
    }
    const double previous_group_energy=group_energy.total;
    // Real main-loop drain is deliberately delayed until several windows queue.
    simulated_ms=counter*1000./rate; c.publish_queued_spi_metering_(); assert(queue.records.empty());
    double expected_group_power=0,expected_group_energy=0;
    for(uint8_t port=0;port<19;++port) {
      const double i1=expected_i1(port),rms=i1*std::sqrt(harmonic ? 1.04 : 1.);
      const double theta=(lag+(port==4 ? 2.5 : 0.))*PI/180.;
      near(fundamental[port].state,i1,.002,.006); near(current[port].state,rms,.002,.006);
      const double signed_theta=std::atan2(orientation*std::sin(theta),orientation*std::cos(theta));
      near(angle[port].state,signed_theta*180./PI,.4);
      near(pf1[port].state,std::fabs(std::cos(theta)),.005);
      near(reactive[port].state,orientation*155.563491861*i1*std::sin(theta),.5,.015);
      near(power[port].state,expected_power(port),.5,.015);
      near(imported[port].state,std::max(expected_power(port),0.),.5,.015);
      near(exported[port].state,std::max(-expected_power(port),0.),.5,.015);
      clamps[port].loop_peak(simulated_ms);
      assert(std::isfinite(peak[port].state) && peak[port].state>=rms);
      near(crest[port].state,peak[port].state/rms,.02);
      const double apparent_expected=155.563491861*std::sqrt(harmonic ? 1.0025 : 1.)*rms;
      near(apparent[port].state,apparent_expected,.5,.01);
      near(pf[port].state,std::fabs(expected_power(port)/(port==4 ? 2. : 1.))/apparent_expected,.012);
      if(harmonic) near(thd[port].state,20.,1.5);
      else assert(std::isnan(thd[port].state) || thd[port].state<8.);
      const double increment=energy[port].total-previous_energy[port];
      near(increment,expected_power(port)*duration/3600000.,.000001,.025);
      // Exact observed physical-window ledger catches even sub-ms timing errors;
      // the independent analytic checks above validate the raw conversion itself.
      near(increment,exact_energy[port],1e-9);
      near(import_energy[port].total,std::max(energy[port].total,0.),1e-9);
      near(export_energy[port].total,std::max(-energy[port].total,0.),1e-9);
      assert(filter_calls[port]-previous_filter_calls[port]==windows);
      assert(std::count_if(power[port].published.begin(),power[port].published.end(),
                          [](float value){return std::isfinite(value);})==drains);
      assert(current[port].published.size()==power[port].published.size());
      assert(reactive[port].published.size()==power[port].published.size());
      assert(energy[port].publications==drains);
      if(port>=3 && port<=5) {expected_group_power+=expected_power(port);expected_group_energy+=increment;}
    }
    near(group_power.state,expected_group_power,1.,.015);
    near(group_energy.total-previous_group_energy,expected_group_energy,1e-6);
    for(uint8_t p=0;p<3;++p) {
      near(voltage[p].state,155.563491861*std::sqrt(harmonic ? 1.0025 : 1.),.2);
      near(frequency[p].state,hz,.02); near(voltage_thd[p].state,harmonic ? 5. : 0.,.15);
    }
  }
};
void test_polarity_frames() {
  for(uint16_t hw:{2,3}) for(auto transport:{MeteringTransport::I2C,MeteringTransport::SPI})
  for(bool pair:{false,true}) for(float correction:{0.f,-3.f,4.f}) {
    Fixture f(hw,50,false); auto &c=f.c;
    Frame frame{}; frame.valid=true; frame.transport=transport; frame.duration_ms=500;
    for(unsigned p=0;p<3;++p) {
      frame.phases[p].voltage_raw=10000;
      frame.phases[p].voltage_fundamental_i_raw=(p==0 ? 10000 : -5000);
      frame.phases[p].voltage_fundamental_q_raw=(p==0 ? 0 : p==1 ? 8660 : -8660);
      frame.phases[p].voltage_fundamental_valid=true;
      frame.phases[p].phase_angle_degrees=p*120.f;
    }
    EmporiaVueComponent::MeteringCTMeasurement normal[19];
    for(unsigned port=0;port<19;++port) {
      auto &ct=f.clamps[port];
      if(pair) ct.set_line_pair(&f.phases[0],&f.phases[1]); else ct.set_phase(&f.phases[0]);
      ct.set_current_phase_correction(transport==MeteringTransport::SPI ? correction : 0);
      ct.set_analysis_interval(0);
      const float scalar=port<3 ? 775.f/42624.f : 775.f/170496.f;
      auto &raw=frame.clamps[port]; raw.current_raw=10.f/scalar;
      raw.current_peak_raw=std::lround(12.f/scalar); raw.current_peak_valid=true;
      raw.current_fundamental_i_raw=3.f/scalar; raw.current_fundamental_q_raw=1.f/scalar;
      raw.current_fundamental_valid=true; raw.power_phase_valid_mask=7;
      raw.power_raw_by_phase[0]=200000; raw.power_raw_by_phase[1]=-150000;
      raw.power_raw_by_phase[2]=-50000;
      assert(c.calculate_ct_measurement_(frame,&ct,&normal[port]));
      if(transport==MeteringTransport::SPI) assert(c.calculate_ct_fundamental_analysis_(frame,&ct,&normal[port]));
      ct.set_polarity_inverted(true);
      EmporiaVueComponent::MeteringCTMeasurement inverted;
      assert(c.calculate_ct_measurement_(frame,&ct,&inverted));
      near(inverted.power,-normal[port].power,.001);
      near(inverted.current,normal[port].current,0);
      near(inverted.apparent_power,normal[port].apparent_power,0);
      near(inverted.power_factor,normal[port].power_factor,0);
      if(transport==MeteringTransport::SPI) {
        assert(c.calculate_ct_fundamental_analysis_(frame,&ct,&inverted));
        near(inverted.fundamental_active_power,-normal[port].fundamental_active_power,.001);
        near(inverted.fundamental_reactive_power,-normal[port].fundamental_reactive_power,.001);
        near(inverted.fundamental_current,normal[port].fundamental_current,0);
        near(inverted.fundamental_power_factor,normal[port].fundamental_power_factor,0);
        near(inverted.current_thd,normal[port].current_thd,0);
        near(std::fabs(std::remainder(inverted.displacement_angle-normal[port].displacement_angle,360.f)),180,.001);
      }
      // I2C int32 extremes must be converted before negation (no signed overflow).
      if(transport==MeteringTransport::I2C) {
        const auto saved=raw.power_raw_by_phase[0]; raw.power_raw_by_phase[0]=INT32_MIN;
        float extreme; assert(c.calculate_ct_physical_power_(frame,&ct,&extreme) && extreme>0);
        raw.power_raw_by_phase[0]=saved;
      }
    }
    unsigned callbacks=0;
    c.frame_callbacks_.add([&](const Frame &raw){
      ++callbacks; assert(raw.clamps[3].power_raw_by_phase[0]==200000);
      assert(raw.clamps[3].current_fundamental_i_raw==frame.clamps[3].current_fundamental_i_raw);
    });
    // Exercise the real publication path, caches, group and signed/directional
    // energy/demand trackers with already inverted physical measurements.
    for(unsigned n=1;n<=16;++n) {
      simulated_ms=n*500; frame.timestamp_ms=simulated_ms; frame.sequence=n;
      c.submit_metering_frame_(frame); // I2C uses successive observation times.
    }
    assert(callbacks==16);
    double group_sum=0;
    for(unsigned port=0;port<19;++port) {
      const auto &base=normal[port];
      near(f.power[port].state,-base.power,.001);
      near(f.imported[port].state,0,0); near(f.exported[port].state,base.power,.001);
      near(f.demand[port].state,-base.power,.001);
      near(f.maximum[port].state,-base.power,.001);
      near(f.current_demand[port].state,base.current,.001);
      if(transport==MeteringTransport::SPI) {
        near(f.export_energy[port].total,-f.energy[port].total,1e-9);
        assert(f.energy[port].total<0 && f.import_energy[port].total==0);
        f.clamps[port].loop_peak(simulated_ms);
        assert(f.peak[port].state>base.current && f.crest[port].state>1);
      }
      if(port>=3 && port<=5) group_sum-=base.power;
    }
    near(f.group_power.state,group_sum,.001);
    std::printf("PASS polarity frames: Vue %u transport=%u pair=%u phase=%.0f, main/branch P/Q/angle/magnitudes/demand/groups/hook\n",
                hw,unsigned(transport),pair,correction);
  }
}
void run_tests() {
  test_polarity_frames();
  for(uint16_t hw:{2,3}) for(float hz:{50.f,60.f}) for(bool harmonic:{false,true})
  for(bool reversed:{false,true}) for(bool inverted:{false,true}) {
    Fixture f(hw,hz,harmonic,reversed,inverted); f.feed(1.5); f.check(1);
    // Same stream, another main-loop stall: durations, not drain timestamps,
    // must determine the increment, and group use must not rerun CT filters.
    f.feed(1.5); f.check(2);
    f.counter+=56; // Missing raw frame: discard partial state, then reacquire.
    f.feed(1.5); f.check(3);
    std::printf("PASS raw scans -> entities + energy: Vue %u %.0fHz %s reversed=%u inverted=%u, 19 inputs, demand/directions/peak/delays/DC/gain/phase/FIFO\n",
                hw,hz,harmonic ? "H3" : "sinus",reversed,inverted);
  }
}
} // namespace entity_test
int main() {entity_test::run_tests();}
"""
    component = "class EmporiaVueComponent { public:\n" + structs + fields + extra
    component += "\n".join(declaration(name) for name in spi_names + math_names) + "\n};\n"
    compile_run(PRELUDE + mocks + constants + classes + helpers + component + trackers +
                "\n".join(bodies) + tests)


if __name__ == "__main__":
    test_spi_entities()
