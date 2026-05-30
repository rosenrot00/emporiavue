# Emporia Vue ESPHome

Local Emporia Vue metering for ESPHome, with a YAML structure that is meant to stay usable as your panel setup grows.

This repository provides an ESPHome external component and ready-to-use packages for Emporia Vue devices. It keeps the
common setup simple, while still giving you clean places for calibration, circuit names, grouped loads, grid
import/export, and more accurate handling of real-world wiring such as line-to-line circuits.

> [!TIP]
> Input and development help are very welcome:
> - Vue 2 accuracy: I am looking for someone who can run accuracy measurements with the adjusted SAMD09 firmware.
> - Vue 2 SAMD09 firmware: ideas, review, and testing around firmware improvements are welcome, including a possible
>   SPI transport.
> - Vue 3: support is not available yet; if you have a Vue 3 and want to help test or map the YAML settings, please get
>   in touch.

## Version History

| Version | Changes |
|---|---|
| 2026.05.1 | Initial public release with [Vue 2 I2C packages](#vue-2-i2c-packages), [runtime voltage calibration](#runtime-calibration), [internal metering filters](#internal-metering-filters), [stable circuit IDs and energy](#stable-circuit-ids-current-and-energy), [apparent power and power factor](#apparent-power-and-power-factor), [groups](#groups), [line-to-line circuit power](#line-to-line-circuits), [virtual lines](#virtual-lines), [windowed phase detection](#phase-detection), [grid import/export](#grid-importexport), [diagnostics](#diagnostics), and [SAMD09 firmware management](#samd09-firmware-management). |

## Setup Examples

Use the Vue 2 I2C base package plus exactly one topology package. Start with the topology that matches how the Vue
voltage inputs are wired, then override only the circuit names, line assignments, filters, and groups that differ in
your panel. Full copy/paste YAML files are collected in [`examples/yaml`](examples/yaml/).

### Vue 2 3phase With Neutral

This is the normal 3phase setup: the Vue neutral terminal is wired to neutral, and the three voltage inputs map to the
three mains lines. This is the setup used by the full example below.

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-i2c.yaml
      - packages/vue2-i2c-3phase.yaml
```

### Vue 2 2phase / Split Phase

Use the 2phase preset when the installation has two measured lines. Keep the base package and swap only the topology
package.

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-i2c.yaml
      - packages/vue2-i2c-2phase.yaml
```

### Vue 2 1phase

Use the 1phase preset for single-line installations. Circuit YAML can still name circuits and add filters, but only one
line is used as the voltage reference.

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-i2c.yaml
      - packages/vue2-i2c-1phase.yaml
```

### Vue 2 Line-to-Line Loads

For loads connected between two measured lines, set the circuit line to a two-item list. The component calculates power
against the voltage difference between those two lines.

```yaml
emporiavue:
  circuits:
    cir2:
      line: [2, 3]
      power:
        name: "Line 2-3 Load Power"
```

### Vue 2 3phase Without Neutral

Some 3phase subpanels do not have neutral available. If the installation has been reviewed and one measured line is
intentionally used as the Vue voltage reference, line-to-line loads can still be calculated. A complete YAML starting
point is available at
[`examples/yaml/vue2-i2c-3phase-no-neutral.yaml`](examples/yaml/vue2-i2c-3phase-no-neutral.yaml).

> [!WARNING]
> This is an advanced electrical setup, not a generic recommendation. Do not rewire the Vue voltage reference unless
> you understand the installation, local electrical rules, and the device safety implications.

If the Vue reference is intentionally wired to `L2`, then `line_1` can represent `L1-L2`, `line_2` is near zero and
usually hidden, and `line_3` can represent `L3-L2`. A load between `L1-L3` can then use `line: [1, 3]`.

```yaml
emporiavue:
  mains:
    line_2:
      voltage:
        internal: true

  circuits:
    cir1:
      line: 1
      power:
        name: "L1-L2 Load Power"
    cir2:
      line: 3
      power:
        name: "L2-L3 Load Power"
    cir3:
      line: [1, 3]
      power:
        name: "L1-L3 Load Power"
```

## Example YAML

A compact Vue 2 3phase setup can look like this. The packages provide the hardware defaults; the node YAML only
names the circuits, applies display filters, defines groups, and adds the energy sensors you want in Home Assistant.

```yaml
esphome:
  name: emporiavue2
  friendly_name: vue2

external_components:
  - source:
      type: git
      url: https://github.com/rosenrot00/emporiavue.git
      ref: main
    components:
      - emporiavue
    refresh: always

packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue.git
    ref: main
    files:
      - packages/vue2-i2c.yaml
      # Pick exactly one topology package:
      # - packages/vue2-i2c-1phase.yaml
      # - packages/vue2-i2c-2phase.yaml
      - packages/vue2-i2c-3phase.yaml
    refresh: always

esp32:
  board: esp32dev
  cpu_frequency: 160MHz
  framework:
    type: esp-idf
    version: recommended

api:
  encryption:
    key: !secret ha_enc_key

ota:
  - platform: esphome
    password: !secret ota_password
    allow_partition_access: true

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  fast_connect: true

logger:
  logs:
    sensor: INFO

preferences:
  flash_write_interval: never

time:
  - platform: sntp
    id: my_time
    timezone: "Europe/Vienna"

.defaultfilters:
  - &throttle_avg
    throttle_average: 5s
  - &throttle_time
    throttle: 60s
  - &pos
    lambda: 'return max(x, 0.0f);'

emporiavue:
  min_apparent_power: 20VA

  phase_detection:
    min_power: 30W
    confidence_ratio: 1.5
    update_interval: 10s

  mains:
    line_1:
      voltage:
        filters: [*throttle_avg, *pos]
      frequency:
        filters: [*throttle_avg, *pos]
      power:
        filters: *throttle_avg

    line_2:
      voltage:
        filters: [*throttle_avg, *pos]
      phase_angle:
        filters: [*throttle_avg, *pos]
      power:
        filters: *throttle_avg

    line_3:
      voltage:
        filters: [*throttle_avg, *pos]
      phase_angle:
        filters: [*throttle_avg, *pos]
      power:
        filters: *throttle_avg

  total_power:
    filters: *throttle_avg

  grid_import_power:
    filters: *throttle_avg

  grid_export_power:
    filters: *throttle_avg

  virtual_lines:
    line_2_3:
      lines: [2, 3]
      voltage:
        filters: [*throttle_avg, *pos]

  circuits:
    cir1:
      line: 2
      power:
        name: "Livingroom Power"
        filters: [*pos, *throttle_avg]
    cir2:
      line: 3
      power:
        filters: *throttle_avg
      current:
        filters: *throttle_avg
      apparent_power:
        filters: *throttle_avg
      power_factor:
        filters: *throttle_avg
    cir3:
      line: 1
      phase_detection: true
      power:
        filters: *throttle_avg
    cir4:
      line: 2
      power:
        filters: *throttle_avg
    cir5:
      line: 3
      power:
        name: "Fridge, Steamer Power"
        filters: *throttle_avg
    cir6:
      line: 3
      power:
        name: "HVAC, Dishwasher Power"
        filters: *throttle_avg

  groups:
    total_heat_pump_power:
      circuits: [cir2, cir3, cir4]
      power:
        name: "Heat Pump Power"
        filters: *throttle_avg

sensor:
  - platform: total_daily_energy
    name: "Today’s Total Energy"
    power_id: total_power
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total
    accuracy_decimals: 2
    restore: true
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Today’s Grid Import Energy"
    power_id: grid_import_w
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    accuracy_decimals: 2
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Today’s Grid Export Energy"
    power_id: grid_export_w
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    accuracy_decimals: 2
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Today’s Heat Pump Energy"
    power_id: total_heat_pump_power
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    accuracy_decimals: 2
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Today’s Livingroom Energy"
    power_id: cir1
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    accuracy_decimals: 2
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Today’s Fridge, Steamer Energy"
    power_id: cir5
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    accuracy_decimals: 2
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Today’s HVAC, Dishwasher Energy"
    power_id: cir6
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    accuracy_decimals: 2
    filters:
      - multiply: 0.001
      - *throttle_time
```

## Feature Details

### Vue 2 I2C Packages

The base package sets up the Vue 2 I2C transport and firmware controls. Add exactly one topology package for your
installation: single-phase, two-phase, or three-phase. The topology package adds the usual line and circuit defaults, so
your node YAML only has to override the parts that are different in your panel.

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-i2c.yaml
      # Pick exactly one topology package:
      # - packages/vue2-i2c-1phase.yaml
      # - packages/vue2-i2c-2phase.yaml
      - packages/vue2-i2c-3phase.yaml
```

### Runtime Calibration

Each main line gets a Home Assistant number entity for calibration. The YAML value is the initial value; once you adjust
it in Home Assistant, ESPHome restores the saved value after reboot.

```yaml
emporiavue:
  mains:
    line_1:
      calibration: 0.022
    line_2:
      calibration: 0.022
    line_3:
      calibration: 0.022
```

### Internal Metering Filters

Filters directly under a main, circuit, CT clamp, or group are metering corrections. They feed energy sensors and group
calculations. Filters under `power` only shape the visible Home Assistant power sensor.

```yaml
emporiavue:
  circuits:
    cir1:
      line: 2
      filters:
        - multiply: -1
      power:
        name: "Livingroom Power"
        filters:
          - throttle_average: 5s
```

### Stable Circuit IDs, Current, and Energy

Every configured circuit gets a stable internal power ID such as `cir1` or `cir5`, even if you do not expose that circuit
as a visible power sensor. That keeps ESPHome energy sensors simple and avoids duplicate display sensors.

Circuits can also expose current directly from the SAMD09 RMS current value. This is not estimated from `power /
voltage`.

```yaml
emporiavue:
  circuits:
    cir2:
      current:
        name: "Circuit 2 Current"
        filters:
          - throttle_average: 5s
```

```yaml
sensor:
  - platform: total_daily_energy
    name: "Today’s Livingroom Energy"
    power_id: cir1
    unit_of_measurement: "kWh"
    device_class: energy
    state_class: total_increasing
    restore: true
    method: left
    filters:
      - multiply: 0.001
```

### Apparent Power and Power Factor

Configured mains, circuits, and legacy `ct_clamps` can expose apparent power and power factor. Apparent power uses the
SAMD09 RMS voltage and RMS current values:

```text
apparent_power = voltage_rms * current_rms
power_factor = real_power / apparent_power
```

For line-to-line circuits, the component uses the calculated line-to-line voltage as the voltage reference. This keeps
`line: [2, 3]` loads consistent with their real voltage reference.

```yaml
emporiavue:
  min_apparent_power: 20VA

  circuits:
    cir2:
      apparent_power:
        name: "Circuit 2 Apparent Power"
        filters:
          - throttle_average: 5s
      power_factor:
        name: "Circuit 2 Power Factor"
        filters:
          - throttle_average: 5s
```

`min_apparent_power` defaults to `20VA`. Below that threshold, apparent power and power factor publish `0` instead of
showing noise-dominated standby values. Set it lower, higher, or to `0VA` if your installation needs a different cutoff.
Power factor is published as a dimensionless magnitude between `0` and `1`; use the real power sensor for direction.

### Groups

Groups create a new power sensor from existing circuit IDs without adding ESPHome template sensors. Use them for
combined loads such as a heat pump across multiple breakers.

```yaml
emporiavue:
  groups:
    total_heat_pump_power:
      circuits: [cir2, cir3, cir4]
      power:
        name: "Heat Pump Power"
        filters:
          - throttle_average: 5s
```

Groups can also subtract sources. This is useful for a balance power sensor that shows the unmonitored remainder:

```yaml
emporiavue:
  groups:
    balance_power:
      circuits: [total_power, -cir1, -cir2, -cir3]
      filters:
        - lambda: |-
            return x > 0.0f ? x : 0.0f;
      power:
        name: "Balance Power"
        filters:
          - throttle_average: 5s
```

### Line-to-Line Circuits

Normal circuits use one line reference, for example `line: 2`. For a load connected between two lines, use a two-item
line list. The component calculates the CT power against the voltage difference between those two lines.

```yaml
emporiavue:
  circuits:
    cir2:
      line: [2, 3]
      power:
        name: "Line 2-3 Load Power"
        filters:
          - throttle_average: 5s
```

If the CT direction or line order gives the opposite sign, add an internal circuit filter:

```yaml
emporiavue:
  circuits:
    cir2:
      line: [2, 3]
      filters:
        - multiply: -1
```

### Virtual Lines

Virtual lines publish derived line-to-line voltage sensors without using or reserving a physical CT port. They use the
configured main line calibration values plus the measured phase offsets, so they work for both split-phase and
three-phase line-to-line voltage display.

```yaml
emporiavue:
  virtual_lines:
    line_2_3:
      lines: [2, 3]
      voltage:
        name: "Line 2-3 Voltage"
        filters:
          - throttle_average: 5s
```

If `name` is omitted, the component uses a stable default such as `Line 2-3 Voltage`.

### Phase Detection

Phase detection is an optional diagnostic helper for single-line branch circuits. It compares the circuit CT against all
available voltage references, collects valid samples for one `update_interval` window, publishes one compact diagnostic
text result, and then starts a fresh window.

```yaml
emporiavue:
  phase_detection:
    min_power: 30W
    confidence_ratio: 1.5
    update_interval: 10s

  circuits:
    cir3:
      phase_detection: true

    cir5:
      phase_detection:
        name: "Heat Pump Phase"
        min_power: 100W
```

If no name is provided, the component derives one from the circuit power name: `Circuit 2 Power` becomes
`Circuit 2 Phase`, and `Heat Pump Power` becomes `Heat Pump Phase`.

The Home Assistant text sensor stays short:

- `low load`: the circuit is below `min_power`.
- `L3 87%`: suggested YAML setting is likely `line: 3`.
- `ambiguous L2/L3`: the best two candidates are too close.

Debug logging includes the details used for the decision, for example the window duration, sample count, and mean
absolute scores per line.
Phase detection is not available for line-to-line circuits because those intentionally use two voltage references.

### Grid Import/Export

The three-phase package exposes total power plus positive-only grid import and export sensors. You can keep their
default names and only add display filters if you want averaged Home Assistant values.

```yaml
emporiavue:
  total_power:
    filters:
      - throttle_average: 5s
  grid_import_power:
    filters:
      - throttle_average: 5s
  grid_export_power:
    filters:
      - throttle_average: 5s
```

### Diagnostics

Diagnostics are optional and stay disabled unless you set `diagnostics_interval`. Use a slow interval such as `30s`;
these entities are meant for checking firmware and transport health, not for normal power dashboards.

```yaml
emporiavue:
  diagnostics_interval: 30s
```

Useful diagnostic entities include:

- `SAMD Packets Built`: metering packets completed by the SAMD09.
- `SAMD Packets Read`: metering packets read by the ESP32.
- `SAMD Packet Overruns`: packets overwritten before the ESP32 read them.
- `SAMD DMA Transfer Errors`: DMA errors reported by the SAMD09.
- `SAMD Last Sample Count`: sample count used for the last completed metering packet.

### SAMD09 Firmware Management

SAMD09 firmware management is an advanced recovery and testing tool. Normal metering does not require pressing any
SAMD flash button.

> [!WARNING]
> Flashing the SAMD09 changes the measurement controller firmware inside the Vue 2. Only use the flash buttons if you
> understand the recovery path and are comfortable working with firmware-level changes.

The Vue 2 I2C package adds a 64 KiB `samd_bak` data partition for SAMD firmware backups. When adding `samd_bak` to a
device that is already flashed, update the ESP32 partition table once. ESPHome documents custom partition lists under
`esp32.partitions`, and partition-table OTA needs `allow_partition_access: true` on the ESPHome OTA platform before
running `esphome upload --partition-table`.

The firmware buttons are configuration entities:

- `Read SAMD Firmware` stores the current SAMD image in `samd_bak`.
- `Flash SAMD Bundled Firmware` installs the firmware image bundled with this component.
- `Flash SAMD Backup Firmware` restores the saved backup image.
- External firmware entries create additional flash buttons for those images.

`auto_update_samd` defaults to `false`. Leave it disabled unless you explicitly want the component to update the SAMD09
automatically when the detected firmware is missing, older, or built for a different transport.

```yaml
emporiavue:
  auto_update_samd: false
  external_samd_firmware:
    - id: vue2_i2c_v1_0
      url: "https://raw.githubusercontent.com/rosenrot00/emporiavue/main/firmware/samd09/images/i2c/vue2-i2c-v1.0.bin"
```

## Vue 2 I2C Packages

The repository currently includes the base `packages/vue2-i2c.yaml` package and three topology presets:
`packages/vue2-i2c-1phase.yaml`, `packages/vue2-i2c-2phase.yaml`, and `packages/vue2-i2c-3phase.yaml`. The base package
sets `hardware: vue2` and `mode: i2c`, adds a 64 KiB `samd_bak` data partition, and enables the firmware version
entities plus the backup, update, and restore buttons. It also uses the component's default `metering_interval: 220ms`
I2C read path that decodes the stock-compatible frame into the component's internal metering frame. The transport is
explicit in the filename so a future SPI transport can live next to it as `packages/vue2-spi.yaml`.

The topology presets create Home Assistant configuration numbers for the main voltage calibration values. The initial
value is `0.022`, matching the old `emporia_vue` component's documented starting point. If a number was changed before,
the restored ESPHome preference wins over the package value on boot.

Keep your `external_components` block in the main node YAML, then include the package:

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-i2c.yaml
      # Pick exactly one topology package:
      # - packages/vue2-i2c-1phase.yaml
      # - packages/vue2-i2c-2phase.yaml
      - packages/vue2-i2c-3phase.yaml
```

## Acknowledgements

This project builds on work from the Emporia Vue local community.

- Thanks to [`emporia-vue-local/esphome`](https://github.com/emporia-vue-local/esphome) for the original ESPHome
  component and local Vue metering work.
- Thanks to [`gekkehenkie11/emporia-SAMD09`](https://github.com/gekkehenkie11/emporia-SAMD09) for publishing a
  stock-compatible SAMD09 firmware reference.
