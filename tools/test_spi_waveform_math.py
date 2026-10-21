#!/usr/bin/env python3
"""Raw SPI scans -> production delay/MUX alignment -> phasors -> metering math.

Only the device/RTOS boundary and configuration objects are mocked. The synthetic
source follows the configured channel-delay contract; physical board timing still
requires a reference instrument.
"""
from test_esphome_metering import HEADER, SPI, METERING, PRELUDE, compile_run, function


def test_waveform_math():
    spi_names = ["reset_spi_metering_state_", "sanitize_spi_adc_offset_", "scale_spi_rms_", "scale_spi_power_",
                 "spi_metering_target_samples_", "spi_metering_target_periods_", "spi_crossing_difference_",
                 "push_spi_fundamental_sample_", "accumulate_spi_voltage_cycle_", "decode_spi_raw_frame_",
                 "process_spi_raw_scan_", "finish_spi_metering_window_"]
    math_names = ["calculate_ct_fundamental_phasors_", "calculate_ct_physical_power_", "calculate_ct_power_",
                  "calculate_phase_voltage_", "calculate_line_to_line_voltage_", "calculate_ct_voltage_",
                  "calculate_ct_current_", "calculate_ct_apparent_power_", "calculate_ct_measurement_",
                  "calculate_ct_fundamental_analysis_"]
    bodies = [function(SPI, "EmporiaVueComponent::" + name) for name in spi_names]
    marker = "  this->spi_metering_accumulator_ = SpiMeteringAccumulator{};\n}"
    assert bodies[-1].endswith(marker)
    bodies[-1] = bodies[-1][:-len(marker)] + "output.push_back(frame);\n" + marker
    bodies += [function(METERING, "EmporiaVueComponent::" + name) for name in math_names]
    declarations = [body[:body.index("{")].replace("EmporiaVueComponent::", "").rstrip() + ";" for body in bodies]
    structs = HEADER[HEADER.index("  struct MeteringPhase {"):HEADER.index("  enum MemSize")].replace("protected:", "public:")
    fields = HEADER[HEADER.index("  SpiMeteringAccumulator spi_metering_accumulator_"):
                    HEADER.index("  volatile uint16_t spi_rx_inflight_")]
    constants = HEADER[HEADER.index("static constexpr float VUE2_STOCK_CYCLE"):
                       HEADER.index("class MeteringPhaseConfig;")]
    helpers = SPI[SPI.index("static constexpr uint8_t SPI_VUE2_RAW_FRAME_VERSION"):
                  SPI.index("void EmporiaVueComponent::publish_spi_diagnostics_")]
    mocks = r"""
namespace sensor {struct Sensor {};}
struct MeteringPhaseConfig {
  uint8_t input{0}; float calibration{0.022f};
  uint8_t get_input_wire() const {return input;}
  float get_calibration() const {return calibration;}
};
struct MeteringCTClampConfig {
  uint8_t port{0}; MeteringPhaseConfig *a{nullptr}, *b{nullptr}; float gain{1}, correction{0};
  std::function<float(float)> filter=[](float x){return x;};
  sensor::Sensor sensor;
  uint8_t get_input_port() const {return port;}
  const MeteringPhaseConfig *get_phase() const {return a;}
  const MeteringPhaseConfig *get_line_pair_phase_b() const {return b;}
  bool is_line_pair() const {return b!=nullptr;}
  float get_current_gain() const {return gain;}
  float get_signed_current_gain() const {return gain;}
  float get_current_phase_correction() const {return correction;}
  float apply_power_filters(float x) const {return filter(x);}
  uint32_t get_analysis_interval() const {return 0;}
  bool has_fundamental_power_analysis() const {return true;}
  sensor::Sensor *get_fundamental_reactive_power_sensor() const {return const_cast<sensor::Sensor*>(&sensor);}
  sensor::Sensor *get_fundamental_power_factor_sensor() const {return const_cast<sensor::Sensor*>(&sensor);}
  sensor::Sensor *get_displacement_angle_sensor() const {return const_cast<sensor::Sensor*>(&sensor);}
  sensor::Sensor *get_current_thd_sensor() const {return const_cast<sensor::Sensor*>(&sensor);}
};
struct VirtualLine {
  const MeteringPhaseConfig *get_line_a() const {return nullptr;}
  const MeteringPhaseConfig *get_line_b() const {return nullptr;}
};
"""
    extra = r"""
  uint16_t hardware_id_{2}; uint32_t metering_interval_ms_{220};
  float spi_sample_rate_hz_{0}, minimum_apparent_power_{5}, minimum_fundamental_current_{0.02f};
  uint32_t spi_last_frame_samples_{0}, spi_invalid_window_last_log_ms_{0};
  std::vector<MeteringPhaseConfig*> metering_phases_;
  std::vector<MeteringCTClampConfig*> metering_ct_clamps_;
  std::vector<VirtualLine*> metering_virtual_lines_;
  std::vector<MeteringFrame> output;
"""
    tests = r"""
constexpr double PI=3.14159265358979323846;
void near(float a,float b,float abs_tol,float rel_tol=0) {
  if (!(std::fabs(a-b)<=abs_tol+std::fabs(b)*rel_tol)) {
    std::fprintf(stderr,"got %.9g expected %.9g (tolerance %.9g)\n",a,b,abs_tol+std::fabs(b)*rel_tol);
    assert(false);
  }
}
int main() {
  for (uint16_t hw : {2,3}) for (float hz : {50.f,60.f}) for (float lag : {-30.f,35.f}) {
    EmporiaVueComponent c; c.hardware_id_=hw;
    const float rate=16000000.f/(hw==2 ? 632.f : 816.f); c.spi_sample_rate_hz_=rate;
    c.spi_main_current_delay_=hw==2 ? 2 : 1; c.spi_mux_current_delay_=hw==2 ? 4 : 2;
    c.spi_waveform_analysis_required_=true; c.spi_current_fundamental_mask_=(1UL<<19)-1;
    c.spi_voltage_sample_ring_.reset(new EmporiaVueComponent::SpiFundamentalSample[c.SPI_VOLTAGE_SAMPLE_RING_SIZE]);
    c.spi_current_peak_mask_=(1UL<<19)-1; c.spi_voltage_thd_mask_=7;
    for (auto &mask : c.spi_power_voltage_mask_) mask=7;
    MeteringPhaseConfig phases[3]{{0},{1},{2}}; MeteringCTClampConfig clamps[19];
    for (auto &p : phases) c.metering_phases_.push_back(&p);
    for (uint8_t port=0;port<19;port++) {
      clamps[port].port=port; clamps[port].a=&phases[port%3]; c.metering_ct_clamps_.push_back(&clamps[port]);
    }
    uint32_t counter=0, sequence=0;
    bool harmonic=false;
    auto phase_at=[&](uint32_t n,uint8_t phase) {return n*2.*PI*hz/rate-phase*2.*PI/3.;};
    auto amplitude=[](uint8_t port) {return port<3 ? 200.+port*10. : 80.+port*3.;};
    auto current=[&](uint32_t n,uint8_t port) {
      const double a=phase_at(n,port%3)-lag*PI/180.;
      return -amplitude(port)*(std::sin(a)+(harmonic ? 0.2*std::sin(3*a) : 0.));
    };
    auto feed=[&](float seconds) {
      const uint32_t frames=seconds*rate/56;
      for (uint32_t f=0;f<frames;f++) {
        uint8_t frame[1024]{};
        for (uint8_t s=0;s<56;s++) {
          uint8_t *data=frame+12+s*18;
          auto put=[&](uint8_t slot,int value) {
            const uint16_t raw=static_cast<uint16_t>(static_cast<int16_t>(value));
            data[slot*2]=raw&255; data[slot*2+1]=raw>>8;
          };
          for (uint8_t p=0;p<3;p++) {
            const double a=phase_at(counter+s,p);
            put(p*2,std::lround(100.+p*17.+1000.*(std::sin(a)+(harmonic ? 0.05*std::sin(3*a) : 0.))));
            put(p*2+1,std::lround(100.+p*7.+current(counter+s+c.spi_main_current_delay_,p)));
          }
          // A received sample is aligned to the voltage scan d positions later.
          const uint8_t future_mux=(counter+s+c.spi_mux_current_delay_)%8;
          for (uint8_t side=0;side<2;side++) {
            const uint8_t internal=future_mux*2+side, port=spi_mux_output_port(hw,internal);
            put(6+side,std::lround(230.+internal*5.+current(counter+s+c.spi_mux_current_delay_,port)));
          }
          data[16]=(counter+s)%8;
        }
        simulated_ms=counter*1000./rate;
        c.spi_sample_rate_hz_=rate; // Frame validation normally refreshes the SAMD timebase.
        c.decode_spi_raw_frame_(frame,sequence++,0,counter); counter+=56;
      }
    };
    feed(1.5); assert(c.output.size()>=5);
    auto check=[&](const EmporiaVueComponent::MeteringFrame &frame,bool with_harmonics) {
      for (uint8_t p=0;p<3;p++) {
        near(frame.phases[p].frequency_hz,hz,0.02);
        near(frame.phases[p].voltage_raw,10000.f/std::sqrt(2.f)*std::sqrt(with_harmonics ? 1.0025f : 1.f),2.);
        if (p) near(frame.phases[p].phase_angle_degrees,p*120.f,0.15f);
        near(frame.phases[p].voltage_thd_percent,with_harmonics ? 5.f : 0.f,0.15f);
      }
      for (uint8_t port=0;port<19;port++) {
        auto &ct=clamps[port]; EmporiaVueComponent::MeteringCTMeasurement m;
        assert(c.calculate_ct_measurement_(frame,&ct,&m));
        assert(c.calculate_ct_fundamental_analysis_(frame,&ct,&m));
        const float scalar=port<3 ? 775.f/42624.f : 775.f/170496.f;
        const float i1=amplitude(port)*10.f*scalar/std::sqrt(2.f);
        near(m.fundamental_current,i1,0.002f,0.006f);
        near(m.current,i1*std::sqrt(with_harmonics ? 1.04f : 1.f),0.002f,0.006f);
        near(m.displacement_angle,lag,0.4f);
        near(m.fundamental_power_factor,std::cos(lag*PI/180.),0.005f);
        near(m.fundamental_reactive_power,155.56349f*i1*std::sin(lag*PI/180.),0.5f,0.015f);
        near(m.power,155.56349f*i1*std::cos(lag*PI/180.),0.5f,with_harmonics ? 0.03f : 0.015f);
        if (with_harmonics) near(m.current_thd,20.f,1.5f);
        else assert(std::isnan(m.current_thd) || m.current_thd<8.f); // Sparse sampling / ADC quantization floor.
        near(m.apparent_power,m.voltage*m.current,0.001f);
        assert(m.has_power_factor && std::isfinite(m.power_factor));
      }
    };
    check(c.output.back(),false);
    {
      // The same physical phasors must also work with a line-pair reference,
      // gain calibration and a runtime phase correction.
      const auto &frame=c.output.back(); auto &ct=clamps[4];
      ct.a=&phases[0]; ct.b=&phases[1]; ct.gain=1.1f; ct.correction=2.5f;
      EmporiaVueComponent::MeteringCTMeasurement m;
      assert(c.calculate_ct_measurement_(frame,&ct,&m));
      assert(c.calculate_ct_fundamental_analysis_(frame,&ct,&m));
      near(m.voltage,155.56349f*std::sqrt(3.f),0.5f);
      const float expected_current=amplitude(4)*10.f*(775.f/170496.f)/std::sqrt(2.f)*1.1f;
      near(m.current,expected_current,0.002f,0.006f);
      float expected_angle=150.f+lag+2.5f;
      if (expected_angle>180.f) expected_angle-=360.f;
      near(m.displacement_angle,expected_angle,0.4f);
      near(m.power,m.voltage*m.fundamental_current*std::cos(expected_angle*PI/180.),0.5f,0.01f);
      ct.a=&phases[4%3]; ct.b=nullptr; ct.gain=1; ct.correction=0;
    }
    harmonic=true; feed(1.0); check(c.output.back(),true);
    // A lost raw frame must discard the partial waveform and re-acquire fully.
    const size_t before=c.output.size(); counter+=56; feed(0.7);
    assert(c.output.size()>before); check(c.output.back(),true);
    std::printf("PASS raw waveform -> math: Vue %u %.0fHz lag %.0f, all 19 inputs, delays, DC, H3 and gaps\n",hw,hz,lag);
  }
  // Fractional RMS survives SPI output and remains consistent with floating I1.
  EmporiaVueComponent precision;
  near(precision.scale_spi_rms_(121,100,25),22.f,1e-6);
  near(precision.scale_spi_rms_(123,100,25),std::sqrt(492.f),1e-6);
  {
    EmporiaVueComponent c; MeteringPhaseConfig phase{0,1}; MeteringCTClampConfig ct; ct.a=&phase; ct.port=3;
    EmporiaVueComponent::MeteringFrame frame; frame.transport=MeteringTransport::I2C;
    frame.phases[0].voltage_raw=100; frame.clamps[3].current_raw=1.f/(775.f/170496.f);
    frame.clamps[3].power_phase_valid_mask=1; frame.clamps[3].power_raw_by_phase[0]=1100;
    EmporiaVueComponent::MeteringCTMeasurement m;
    ct.filter=[](float x){return x+10;}; c.calculate_ct_measurement_(frame,&ct,&m);
    near(m.power,60,0.001); near(m.apparent_power,100,0.001); near(m.power_factor,0.5,0.001);
    ct.filter=[](float x){return -x*2;}; c.calculate_ct_measurement_(frame,&ct,&m);
    near(m.power,-100,0.001); near(m.apparent_power,100,0.001); near(m.power_factor,0.5,0.001);
    frame.clamps[3].power_phase_valid_mask=0; c.calculate_ct_measurement_(frame,&ct,&m);
    assert(!m.has_power_factor && std::isnan(m.power_factor));
    frame.clamps[3].power_phase_valid_mask=1; frame.clamps[3].power_raw_by_phase[0]=2300;
    c.calculate_ct_measurement_(frame,&ct,&m); assert(!m.has_power_factor && std::isnan(m.power_factor));
    frame.clamps[3].power_raw_by_phase[0]=2201; c.calculate_ct_measurement_(frame,&ct,&m);
    assert(m.has_power_factor); near(m.power_factor,1,0.001); // Tiny quantization/calibration tolerance.
    frame.clamps[3].current_raw=0; c.calculate_ct_measurement_(frame,&ct,&m);
    assert(!m.has_power_factor && std::isnan(m.power_factor));
    frame.clamps[3].current_raw=NAN; c.calculate_ct_measurement_(frame,&ct,&m); assert(!m.has_current);
  }
  std::puts("PASS physical S/PF: output filters isolated; missing, low-load and inconsistent data unknown");
}
"""
    compile_run(PRELUDE + mocks + constants + helpers + "\nclass EmporiaVueComponent {public:\n" +
                structs + fields + extra + "\n".join(declarations) + "\n};\n" + "\n".join(bodies) + tests)


if __name__ == "__main__":
    test_waveform_math()
