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
| 2026.05.1 | Initial public release with [Vue 2 I2C packages](#vue-2-i2c-packages), [runtime voltage calibration](#runtime-calibration), [internal metering filters](#internal-metering-filters), [stable circuit IDs and energy](#stable-circuit-ids-and-energy), [groups](#groups), [line-to-line circuit power](#line-to-line-circuits), [grid import/export](#grid-importexport), and [SAMD09 firmware management](#samd09-firmware-management). |

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
  metering_interval: 220ms

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
    cir3:
      line: 1
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

### Stable Circuit IDs and Energy

Every configured circuit gets a stable internal power ID such as `cir1` or `cir5`, even if you do not expose that circuit
as a visible power sensor. That keeps ESPHome energy sensors simple and avoids duplicate display sensors.

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

### SAMD09 Firmware Management

Firmware management is optional. It is available for reading, flashing, restoring, or testing SAMD09 images when you
explicitly need it.

> [!WARNING]
> Flashing the SAMD09 changes the measurement controller firmware inside the Vue 2. Only use the flash buttons if you
> understand the recovery path and are comfortable working with firmware-level changes.

The Vue 2 I2C package adds a 64 KiB `samd_bak` data partition for SAMD firmware backups. When adding `samd_bak` to a
device that is already flashed, update the ESP32 partition table once. ESPHome documents custom partition lists under
`esp32.partitions`, and partition-table OTA needs `allow_partition_access: true` on the ESPHome OTA platform before
running `esphome upload --partition-table`.

SAMD writes are enabled by default, and updating the managed SAMD firmware does not require a legacy backup. The
firmware buttons are configuration entities: `Read SAMD Firmware` stores the current SAMD image in `samd_bak`,
`Flash SAMD Bundled Firmware` installs the bundled managed image, and `Flash SAMD Backup Firmware` restores the saved
backup image. If external firmware is configured, an additional external flash button is generated for that image.

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
entities plus the backup, update, and restore buttons. It also uses the component's default `metering_interval: 200ms`
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
