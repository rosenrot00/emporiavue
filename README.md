# Emporia Vue ESPHome

Turn an Emporia Vue into a local ESPHome energy meter for Home Assistant. Start with dependable ESPHome I2C metering,
or use the ESPHome SPI path when you specifically want synchronized raw-waveform analysis.

## Version History

| Version | Changes |
|---|---|
| 2026.07.10 | Separated direction-aware line diagnostics from automatic line assignment and added import/export auto modes. |
| 2026.07.9 | Added native ESPHome subdevices, validated Vue 3 SPI on real hardware, and added optional SPI voltage THD. |
| 2026.07.8 | Added persistent automatic circuit line assignment with an optional Home Assistant line selector. |
| 2026.07.7 | Renamed voltage calibration options and added optional per-CT current gain and SPI phase calibration. |
| 2026.07.6 | Added time-windowed SPI current peak and current crest factor entities. |
| 2026.07.5 | Added configurable rolling power/current demand and daily maximum demand for mains, circuits, and groups. |
| 2026.07.4 | Added sample-derived SPI line-to-line RMS voltage plus optional fundamental current, fundamental reactive power, fundamental power factor, displacement angle, and current THD entities. |
| 2026.07.3 | Improved SPI frequency and phase-angle stability with complete interpolated line cycles and a shared period reference. |
| 2026.07.2 | Added fundamental-voltage phase measurement over the SPI metering window. |
| 2026.07.1 | Defaulted simple daily energy sensors to `total_increasing`; explicit signed/net energy remains `total`. |
| 2026.06.3 | Fixed Vue 3 physical main-clamp mapping. |
| 2026.06.2 | Improved stock I2C frame marker and checksum compatibility. |
| 2026.06.1 | Added the initial ESPHome SPI transport for Vue 2 and display filter defaults. |
| 2026.05.1 | Initial package-based release. |

## Choose Your Path

|  | Emporia stock | ESPHome I2C | ESPHome SPI |
|---|---|---|---|
| **Best for** | Official Emporia experience | Normal daily monitoring | Enthusiasts and development |
| **You get** | Emporia app and cloud | Local Home Assistant entities | Local entities plus waveform detail |
| **Measurements** | Official Emporia feature set | Voltage, current, power, energy, demand, groups and import/export | Same core values plus optional fundamental and waveform analysis |
| **Firmware** |  | Stock SAMD09 firmware works on Vue 2 and Vue 3; the Vue 2 custom firmware calculates line-to-line voltage without a fixed `√3` assumption | Model-specific managed SAMD09 firmware required |

The short decision is:

1. Want the official Emporia app and cloud? **Stay with Emporia stock.**
2. Want reliable local values in Home Assistant? **Choose ESPHome I2C.** This is the right default for most users.
3. Want waveform-derived values such as reactive power, displacement angle, or current THD on a Vue 2 or Vue 3?
   **Choose ESPHome SPI.** It uses the matching model-specific managed SAMD09 firmware.

## Quick Start

Every node combines exactly two core packages:

- one **base package** selects the Vue model and ESPHome I2C/SPI path;
- one **topology package** describes the connected voltage inputs.

### 1. Choose ESPHome I2C or ESPHome SPI

#### ESPHome I2C — recommended for normal use

Choose this when you want a straightforward local meter. It provides the everyday values most dashboards need without
requiring raw waveform processing.

| Device | Base package | What you get |
|---|---|---|
| Vue 2 | `packages/vue2-i2c.yaml` | Local metering through the stock-compatible I2C interface |
| Vue 3 | `packages/vue3-i2c.yaml` | Local metering through the stock-compatible I2C interface |

#### ESPHome SPI — optional enhanced metering

Choose the SPI base package matching your device when you want sample-derived metering and the optional analysis
entities described below.

| Device | Base package | What you get |
|---|---|---|
| Vue 2 | `packages/vue2-spi.yaml` | Raw-sample metering and optional waveform analysis |
| Vue 3 | `packages/vue3-spi.yaml` | Raw-sample metering and optional waveform analysis; validated on real hardware |

| Benefit | Practical result |
|---|---|
| Raw synchronized voltage/current samples | Active power is calculated directly from the waveform |
| Direct line-to-line waveform calculation | Better handling of distorted line-to-line voltages |
| Fundamental phasors | Optional fundamental current, reactive power, PF and displacement angle |
| Total RMS versus fundamental current | Optional estimated current THD |

### 2. Choose the electrical topology

Choose the topology from the voltage inputs that are actually connected:

| Installation | Vue 2 topology | Vue 3 topology |
|---|---|---|
| One measured line | `packages/vue2-1phase.yaml` | `packages/vue3-1phase.yaml` |
| Two measured lines / split phase | `packages/vue2-2phase.yaml` | `packages/vue3-2phase.yaml` |
| Three phases with neutral | `packages/vue2-3phase.yaml` | `packages/vue3-3phase.yaml` |

Example: Vue 2 with I2C and three phases:

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-i2c.yaml
      - packages/vue2-3phase.yaml
```

For ESPHome SPI, replace only the base package:

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    files:
      - packages/vue2-spi.yaml
      - packages/vue2-3phase.yaml
```

For a Vue 3, use `packages/vue3-spi.yaml` together with the matching Vue 3 topology package.

### 3. Add the external component

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/rosenrot00/emporiavue.git
      ref: main
    components: [emporiavue]
```

### 4. Name and assign circuits

The topology packages define all 16 branch inputs. Your node YAML only overrides the entries that differ in your panel.
A circuit is visible in Home Assistant only when you add a sensor such as `power:`, `current:`, or `energy:`.

```yaml
emporiavue:
  circuits:
    cir1:
      # No name: keeps the default base name "Circuit 1"
      line: 1
      power:
      energy:

    cir2:
      name: "Heat Pump"
      line: 2
      power:
      current:
      power_apparent:
      power_factor:
```

Without `name:`, the topology package keeps the default circuit name, for example `Circuit 1`. With the default native
subdevices, Home Assistant already supplies that device context, so a device named `Heat Pump` contains concise entity
names:

```text
Power
Current
Apparent Power
Power Factor
```

This produces entity IDs such as `sensor.heat_pump_power`, without repeating the device name. If
`esphome_subdevices: false` is used, the complete names such as `Heat Pump Power` remain on the central device. An
explicit sensor `name:` always wins over both the default and the circuit name.

By default, every main line, circuit, and group with at least one visible entity is exposed as its own native ESPHome
subdevice in Home Assistant. Each logical line keeps its voltage, main-CT measurements, calibration controls, and
optional SPI analysis together. Circuit measurements stay under the circuit name; group totals such as `Grid`,
`Wallbox`, or `Unmonitored` stay together as well. Entries without visible entities do not create empty devices. The
central Emporia Vue device retains firmware controls and general diagnostics. To keep every entity on that central
device instead, disable the feature globally:

```yaml
emporiavue:
  esphome_subdevices: false
```

To keep the source measurements and the group totals on one Home Assistant device, set
`sources_to_subdevice: all` on that group:

```yaml
emporiavue:
  circuits:
    cir2:
      name: "Heat Pump L1"
      power:
      current:

    cir3:
      name: "Heat Pump L2"
      power:
      current:

    cir4:
      name: "Heat Pump L3"
      power:
      current:

  groups:
    heat_pump:
      name: "Heat Pump"
      sources: [cir2, cir3, cir4]
      sources_to_subdevice: all
      power:
      energy:
```

The `Heat Pump` device then contains aggregate entities such as `Power` and `Today's Energy`, plus source entities such
as `L1 Power`, `L1 Current`, `L2 Power`, and `L3 Power`. Their IDs remain concise, for example
`sensor.heat_pump_power` and `sensor.heat_pump_l1_power`.

Use a list when only selected sources should move while the remaining sources keep their own subdevices:

```yaml
groups:
  combined_load:
    sources: [cir2, cir3, cir4, cir5]
    sources_to_subdevice: [cir2, cir3, cir4]
```

`sources_to_subdevice` is optional. `all` moves every direct source; a list moves only those entries. `true` remains an
alias for `all`, and `false` is equivalent to leaving the option out. The selected source entities move to the group
subdevice and their separate source subdevices disappear; entities are not duplicated. This works for main lines,
circuits, and other groups. For example, using `all` on the predefined `grid` group puts the three line measurements and
the Grid totals on the same device. The group device is also created when its only visible entities come from its
sources. Every selected entry must occur in the group's `sources`, and a source can be moved to only one group. A `-`
sign in `sources`, such as `-cir1`, affects the calculation but not the entity placement. The option is ignored when
`esphome_subdevices` is disabled. Automatically generated names are made relative to the receiving subdevice; source
details needed to distinguish multiple circuits are retained. ESPHome's normal validation rejects conflicting entity
names instead of creating an ambiguous or duplicated entity ID.

### 5. Understand `line`

`input` is the physical CT socket. `line` selects the configured voltage reference used for that CT.
All circuits default to logical `line: 1`; adjust each circuit to match the actual line used in your installation.

```yaml
cir1:
  line: 1
```

`line_1` means the first configured voltage input, normally `voltage_input: BLACK`. It does not magically identify the
utility label printed on the conductor. If the black Vue voltage lead is physically connected to L2, logical `line_1`
measures that real L2. Assign every circuit from the actual installation rather than assuming CT socket order determines
the phase.

If you do not know the correct line, use `line: auto_import` for a consuming circuit or `line: auto_export` for a
generating circuit. Line detection first records the current operating state as a reference, then waits for a clear load
change. After three stable change windows it applies the detected line and stores it for the next restart. Automatic
assignment does not require a Home Assistant entity:

```yaml
cir2:
  name: "Heat Pump"
  line: auto_import
```

For a circuit that normally feeds power back:

```yaml
cir2:
  name: "Solar"
  line: auto_export
```

`line: auto` remains a short alias for `line: auto_import`.

Add `line_select:` only when Home Assistant should also provide a dropdown:

```yaml
cir2:
  name: "Heat Pump"
  line: auto_import
  line_select:
```

The optional `Heat Pump Line` selector offers `Auto Import`, `Auto Export`, and every configured line, for example
`L1`, `L2`, and `L3`. It stays on the selected automatic mode until detection is reliable and then changes to the
detected line. Choosing an automatic mode later starts a new detection; choosing a line applies and stores it
immediately.

| Circuit YAML | Assignment | Home Assistant dropdown |
|---|---|---|
| `line: 1` | Fixed by YAML | No |
| `line: auto_import` or `auto_export` | Detected once and stored | No |
| `line: 1` plus `line_select:` | Starts with L1; stored dropdown choice wins | Yes |
| Automatic `line` plus `line_select:` | Starts in the requested auto mode; changes to the detected line | Yes |

`line_select:` is only the optional Home Assistant control, just like `voltage_calibration_number:`. Removing it makes
a numeric `line:` authoritative again. With an automatic `line`, detection and storage continue to work without the
dropdown.

On the first automatic run, phase-dependent values remain unknown until a line is detected; current values remain
available. Once detected, the stored line is restored immediately after subsequent restarts. Storage follows the
circuit key such as `cir2`, so changing its name or moving it to another subdevice does not reset the assignment.

For a load connected between two lines, use a pair:

```yaml
cir8:
  name: "Wallbox"
  line: [1, 2]
  power:
  power_apparent:
  power_factor:
```

## Optional ESPHome SPI Analysis

ESPHome SPI exposes the raw voltage and current sample stream. The component can therefore separate the fundamental
component from the total RMS waveform and optionally publish additional analysis entities per main or branch CT.

### Voltage THD

Voltage THD is configured on a main voltage reference, not on a circuit:

```yaml
emporiavue:
  filter_defaults:
    voltage_thd:
      - throttle_average: 10s

  mains:
    line_1:
      voltage_thd:
    line_2:
      voltage_thd:
    line_3:
      voltage_thd:
```

This creates entities such as `Line 1 Voltage THD`. Only explicitly configured entities are created. A local
`filters:` entry on one `voltage_thd:` sensor replaces the global default, just like the other sensor types.

The SPI path measures complete cycles synchronized to the detected grid frequency. For each requested voltage input it
calculates the RMS components of harmonics 2 through 40 and publishes:

```text
Voltage THD = sqrt(U2² + U3² + ... + U40²) / U1 × 100%
```

`U1` is the voltage fundamental. When the fundamental is unavailable or too small for a valid analysis, the entity is
`unknown`. Voltage calibration scales the fundamental and harmonics equally and therefore does not change the THD
ratio. Harmonic processing is only performed for voltage inputs that have a `voltage_thd:` entity. The result is a
waveform-derived diagnostic value; absolute accuracy should be checked against a suitable reference instrument when it
matters.

`throttle_average` is recommended for display smoothing. The SPI analysis still uses every complete synchronized
measurement window; the ESPHome filter only controls how often the averaged result is published.

### Circuit waveform analysis

```yaml
emporiavue:
  minimum_apparent_power: 5VA
  minimum_fundamental_current: 20mA
  peak_interval: 5s

  circuits:
    cir2:
      name: "Heat Pump"
      line: 2

      fundamental_current:
      fundamental_reactive_power:
      fundamental_power_factor:
      displacement_angle:
      current_thd:
      current_peak:
      current_crest_factor:
```

This creates:

```text
Heat Pump Fundamental Current
Heat Pump Fundamental Reactive Power
Heat Pump Fundamental Power Factor
Heat Pump Displacement Angle
Heat Pump Current THD
Heat Pump Current Peak
Heat Pump Current Crest Factor
```

The keys are optional; only configured entities are created. They are rejected during YAML validation when `mode: i2c`
is selected.

| YAML key | Unit | Meaning |
|---|---:|---|
| `fundamental_current` | A | RMS current of the fundamental component |
| `fundamental_reactive_power` | var | Signed fundamental reactive power; with correct CT orientation, positive is inductive and negative is capacitive for normal import |
| `fundamental_power_factor` | — | Fundamental power factor `abs(P1) / S1`, from 0 to 1 |
| `displacement_angle` | ° | Voltage-current displacement angle; with correct CT orientation, positive is lagging/inductive for normal import |
| `current_thd` | % | Estimated residual current relative to the fundamental current |
| `current_peak` | A | Highest sampled absolute current during the completed peak interval |
| `current_crest_factor` | — | Highest waveform crest factor `Current Peak / RMS Current` during the completed peak interval |

At low current, a PF, angle, or THD number would be dominated by noise. The behavior is therefore deliberate:

```text
Fundamental Current          0 A
Fundamental Reactive Power   0 var
Fundamental Power Factor     unknown
Displacement Angle           unknown
Current THD                  unknown
```

`minimum_fundamental_current` controls that boundary globally. `minimum_apparent_power` is the global cutoff for the
existing apparent-power and total-power-factor outputs.

`peak_interval` controls a separate time window for `current_peak` and `current_crest_factor`. The default is `5s`, and
supported values are 1 to 60 seconds. Each complete SPI metering window is about 220 ms. During the peak interval, the
component keeps the highest current peak and the highest crest factor and publishes them only when the interval ends.
The same `minimum_fundamental_current` threshold suppresses noise-only results: below it, Current Peak is `0 A` and
Current Crest Factor is `unknown`. A local `peak_interval` on a main or circuit overrides the global value.

> [!NOTE]
> Existing configurations from before version 2026.07.4 must rename `power_apparent_min` to
> `minimum_apparent_power`.

## A Practical Node Example

This example keeps the node YAML focused on user choices. The selected packages provide hardware pins, firmware
handling, voltage references, all 16 CT inputs, and stable internal IDs.

```yaml
esphome:
  name: emporiavue2
  friendly_name: Vue 2

external_components:
  - source:
      type: git
      url: https://github.com/rosenrot00/emporiavue.git
      ref: main
    components: [emporiavue]

packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue.git
    ref: main
    files:
      - packages/vue2-spi.yaml
      - packages/vue2-3phase.yaml

esp32:
  board: esp32dev
  framework:
    type: esp-idf
    version: recommended

api:

ota:
  - platform: esphome
    allow_partition_access: true
    on_begin:
      then:
        # Store related restored values together before the OTA reboot.
        - lambda: global_preferences->sync();

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
  logs:
    sensor: INFO

preferences:
  # Avoid periodic flash writes; persistent state is stored at controlled sync points.
  flash_write_interval: never

time:
  - platform: sntp
    id: my_time

.display_filters:
  - &fast_average
    throttle_average: 5s
  - &slow_update
    throttle: 60s
  - &analysis_average
    throttle_average: 10s

emporiavue:
  minimum_apparent_power: 5VA
  minimum_fundamental_current: 20mA
  peak_interval: 5s

  filter_defaults:
    voltage: [*fast_average]
    frequency: [*fast_average]
    phase_angle: [*fast_average]
    power: [*fast_average]
    current: [*fast_average]
    power_apparent: [*fast_average]
    power_factor: [*fast_average]
    fundamental_current: [*fast_average]
    fundamental_reactive_power: [*fast_average]
    fundamental_power_factor: [*fast_average]
    displacement_angle: [*analysis_average]
    current_thd: [*analysis_average]
    voltage_thd: [*analysis_average]
    energy:
      - multiply: 0.001
      - *slow_update

  mains:
    # Each Number is initialized from the corresponding package calibration.
    # voltage_thd is available only with ESPHome SPI.
    line_1:
      voltage_calibration_number:
      voltage_thd:

    line_2:
      voltage_calibration_number:
      voltage_thd:

    line_3:
      voltage_calibration_number:
      voltage_thd:

  circuits:
    cir1:
      name: "Living Room"
      line: 1
      power:
      energy:

    cir2:
      name: "Heat Pump"
      line: auto_import
      # Optional diagnostic; it follows auto_import and never changes line itself.
      line_detection:
      # Optional Home Assistant dropdown; automatic or manual choices are stored.
      line_select:
      power:
      current:
      power_apparent:
      power_factor:
      # Gain works with I2C and SPI; phase correction is SPI-only.
      current_calibration:
        gain: 1.0
        phase: 0°
        # Optional persistent Home Assistant Numbers.
        gain_number:
        phase_number:  # SPI-only, like phase.
      # SPI-only waveform analysis.
      fundamental_current:
      fundamental_reactive_power:
      fundamental_power_factor:
      displacement_angle:
      current_thd:
      current_peak:
      current_crest_factor:

    cir8:
      name: "Wallbox L1"
      line: 1
      power:
      current:

    cir9:
      name: "Wallbox L2"
      line: 2
      power:
      current:

    cir10:
      name: "Wallbox L3"
      line: 3
      power:
      current:

  groups:
    wallbox:
      name: "Wallbox"
      sources: [cir8, cir9, cir10]
      sources_to_subdevice: all
      power:
      energy:

    grid:
      name: "Grid"
      sources: [line_1, line_2, line_3]
      sources_to_subdevice: all
      power:
        both:
          name: "Grid Net Power"
        positive:
          name: "Grid Import Power"
          energy:
        negative:
          name: "Grid Export Power"
          energy:
```

Keep `flash_write_interval: never` in the node configuration. It prevents periodic background preference writes,
reduces flash wear, and helps keep related restored values consistent by storing them at deliberate synchronization
points. The example performs one such synchronization before OTA. EmporiaVue calibration inputs and line selectors also
synchronize immediately when changed.

The Wallbox example uses one CT per phase. Its `sources_to_subdevice` option places the three phase currents and powers
on the same Home Assistant device as the summed power and energy. The Grid example similarly combines the three main
lines with the Grid totals. A single CT with `line: [1, 2]` represents a two-wire line-to-line load, not a complete
three-phase load.

Full and specialized examples are available in [`examples/yaml`](examples/yaml/).

## Common Configuration Recipes

### Display filters and metering corrections

`filter_defaults` applies normal ESPHome display filters only to sensors that you explicitly create. It does not create
entities by itself. A local sensor `filters:` entry replaces the corresponding default.

```yaml
emporiavue:
  filter_defaults:
    power:
      - throttle_average: 5s
    current_thd:
      - throttle_average: 10s
    voltage_thd:
      - throttle_average: 10s

  circuits:
    cir1:
      power:
      current_thd:
        filters:
          - throttle_average: 30s
```

`filters` directly under a circuit are internal active-power corrections used by power, energy, and groups:

```yaml
cir1:
  filters:
    - multiply: -1
  power:
```

The fundamental analysis deliberately does not reuse arbitrary internal active-power filters. Applying a nonlinear
filter independently to P1, Q1, and S1 would destroy the phasor relationship and could hide negative/capacitive Q.
Normal display filters directly on the new sensor entities are supported.

Avoid `max(x, 0)` or absolute-value display filters on signed reactive power or displacement angle unless you explicitly
want to discard direction.

### Energy

Add `energy:` next to a circuit to create an energy entity; a visible `power:` sensor is not required.

```yaml
cir1:
  name: "Living Room"
  energy:
    # state_class: total  # Uncomment for explicit signed/net energy.
    filters:
      - multiply: 0.001
```

Simple daily energy defaults to `state_class: total_increasing`. Explicit signed/net energy can use `state_class: total`.
`both`, `positive`, and `negative` are power directions, not energy modes. A nested `energy:` integrates that selected
output: `both` is signed/net, `positive` keeps positive power, and `negative` exposes negative power as a positive value.
An `energy:` directly under the circuit uses `both`. Use separate positive/import and negative/export energy rather than
signed net energy in the Home Assistant Energy Dashboard.

### Demand

Demand is the time-weighted average power or RMS current over a moving interval. The default is 15 minutes. Set
`demand_interval` globally, or override it for an individual main, circuit, or group. A local value wins over the global
value; when neither is set, 15 minutes is used. Supported intervals are 1 to 60 minutes.

```yaml
emporiavue:
  demand_interval: 15min

  circuits:
    cir2:
      name: "Heat Pump"
      power_demand:
      maximum_power_demand:
      current_demand:
      maximum_current_demand:

    cir8:
      name: "Wallbox"
      demand_interval: 5min
      power_demand:
      maximum_power_demand:

  groups:
    grid:
      sources: [line_1, line_2, line_3]
      power_demand:
      maximum_power_demand:
```

This creates names such as `Heat Pump Power Demand` and `Today's Heat Pump Maximum Power Demand`. The rolling demand
does not reset; it always represents the latest complete interval and stays `unknown` until that first interval is
available. The daily maximum resets at midnight and only starts again after a complete interval from the new day. Like
daily energy, its state is restored after a restart by default. The maximum entities need an ESPHome `time:` source.
All Demand entities are optional and work with both I2C and SPI.

### Groups

Groups sum or subtract mains, circuits, and other groups without ESPHome template sensors.

```yaml
emporiavue:
  groups:
    heat_pump:
      name: "Heat Pump"
      sources: [cir2, cir3, cir4]
      power:
      energy:

    unmonitored:
      name: "Unmonitored"
      sources: [grid, -cir1, -cir2, -cir3]
      power:
```

### Grid import and export

Topology packages define `grid` as an internal group. Add directional outputs only when you want visible entities:

```yaml
emporiavue:
  groups:
    grid:
      sources: [line_1, line_2, line_3]
      power:
        both:
        positive:
          energy:
        negative:
          energy:
```

Positive is import. Negative is exposed as a positive export value on the `negative` output.

### Line-to-line voltage and power

For `line: [1, 2]`, real power is calculated from the instantaneous voltage difference:

```text
p[n] = (v1[n] - v2[n]) * i[n]
```

In SPI mode, line-to-line RMS voltage is also calculated directly from the waveform:

```text
V12_rms = sqrt(mean((v1[n] - v2[n])²))
```

This improves line-to-line apparent power and PF when the two voltage waveforms contain different harmonic content.
I2C has no raw samples, so it retains the RMS/phase-angle phasor reconstruction as a fallback.

### Power split

`power_split` is a presentation helper for dashboards. It publishes half of one measured line-to-line circuit power on
each selected line; it is not a separate conductor measurement.

```yaml
cir8:
  line: [1, 2]
  power:
  power_split:
    line_1:
    line_2:
```

### Virtual line voltage

Virtual lines expose a line-to-line voltage without reserving a CT:

```yaml
emporiavue:
  virtual_lines:
    line_2_3:
      lines: [2, 3]
      voltage:
```

### Runtime voltage calibration

`voltage_calibration` is the authoritative YAML value. Add `voltage_calibration_number:` only when that line should also
have an adjustable Home Assistant Number. The YAML value initializes the Number; a value changed in Home Assistant is
stored and restored after reboot. Storage follows the logical line such as `line_1`, not its name or subdevice.

```yaml
emporiavue:
  mains:
    line_1:
      voltage_calibration: 0.022
      voltage_calibration_number:
        name: "Line 1 Voltage Calibration"
```

Voltage calibration affects voltage and all power quantities using that voltage reference. Validate changes against a
trusted meter and a known load.

Existing configurations must rename `calibration` to `voltage_calibration`. Runtime adjustment is no longer created
automatically; add `voltage_calibration_number:` explicitly when it is wanted.

### Current calibration

Current calibration is optional and belongs directly to a main or circuit. `gain` works with I2C and SPI. `phase` is an
SPI-only correction added to the measured fundamental current angle.

```yaml
emporiavue:
  mains:
    line_1:
      current_calibration:
        gain: 1.005

  circuits:
    cir2:
      name: "Heat Pump"
      current_calibration:
        gain: 1.012
        phase: -0.35°
        gain_number:
        phase_number:
```

When `current_calibration` is absent, `gain: 1.0` and `phase: 0°` are implied. `gain_number` and `phase_number` are
optional persistent Home Assistant controls; without them, only YAML is used. Gain consistently scales current, power,
apparent power, reactive power, peak, demand, energy, and groups. On SPI, phase calibration rotates the fundamental
current phasor and corrects the fundamental contribution to active power, keeping P, Q, PF, and displacement angle
consistent. Stored values follow the main or circuit key such as `line_1` or `cir2`; names and subdevice assignments can
change without resetting them. Leave the defaults unchanged without a trusted meter and a suitable reference load.

### Line detection helper

Line detection compares a single-line CT with all configured voltage references and suggests the most likely logical
line. `line_detection` only creates the diagnostic result: it never changes or stores the circuit assignment. Use
`line: auto_import` or `line: auto_export` when the component should also apply and store a detected line.

```yaml
emporiavue:
  line_detection:
    power_min: 30W
    update_interval: 10s

  circuits:
    cir3:
      line: 1
      line_detection:
```

With a fixed `line`, an empty `line_detection:` defaults to `import`. With `line: auto_import` or `line: auto_export`,
it inherits that automatic direction. Set `line_detection: import` or `line_detection: export` to choose the diagnostic
direction explicitly. An explicit diagnostic direction may differ from the automatic assignment direction:

```yaml
cir3:
  line: auto_import
  line_detection: export
```

Here automatic assignment independently detects, applies, and stores the import line. The visible diagnostic observes
export operation without ever changing the assignment. When both use the same direction they still have independent
state, so the diagnostic remains active after automatic assignment has finished.

The first complete window becomes the reference state; it may be standby, full load, or anything in between. Detection
then evaluates the signed change from that reference. Both load increases and decreases are supported, and `power_min`
is the minimum required correlation change rather than a minimum absolute circuit load.

The expected line must change by at least `power_min` in the configured direction. `confidence_ratio` controls how
clearly its correlation must exceed the next-best positive candidate and defaults to `1.5`; normally it does not need to
be configured. This guard band accepts a dominant line even for moderately phase-shifted motor loads, while remaining
ambiguous near a phase boundary. The independently measured RMS current must also confirm that an actual load transition
occurred. A doubtful measurement is never stored.

Possible text states are `waiting for change`, `ambiguous change`, `L2 weak`, `L2`, or `ambiguous L2/L3`. It is
intentionally unavailable for line-to-line circuits.

No particular startup state is required, but the circuit must change operating state at least once. For example, let a
heat pump start or stop and then keep the new state steady. `waiting for change` means no sufficiently large transition
has occurred. `ambiguous change` means the current changed but not in a way that safely identifies a physical line.
`ambiguous L2/L3` means the direction or phase displacement is still too close to a decision boundary. `L3 weak` is a
preliminary result. A stable result needs three consecutive update windows, so with the defaults the new state should
remain steady for about 30 seconds. If the result is `L3`, set that circuit to `line: 3` (`L1` means `line: 1`, and so
on). The result remains visible while the detector quietly re-arms at the new operating point. An unresolved transition
is reported once and also becomes the new reference, rather than being evaluated repeatedly. The detector intentionally
waits for the next real change instead of guessing. After assigning the line, you can remove `line_detection:` if you no
longer want the visible diagnostic; automatic assignment is controlled only by `line`.

### Three phase without neutral

There is no universal no-neutral preset because the voltage reference depends on the installation. Start with
[`examples/yaml/vue2-3phase-no-neutral.yaml`](examples/yaml/vue2-3phase-no-neutral.yaml) only if you understand the
wiring, safety implications, and line-to-line calculation.

## How the SPI Analysis Works

This section is for users who want to understand what the new entities represent.

### Total waveform measurements

SPI metering uses centered raw samples over complete mains cycles:

```text
U_rms = sqrt(mean(u[n]²))
I_rms = sqrt(mean(i[n]²))
P     = mean(u[n] * i[n])
S     = U_rms * I_rms
PF    = abs(P) / S
```

`P` is true active power from sample correlation, not an estimate from `U * I * cos(phi)`. Non-sinusoidal current is
therefore included in total RMS, active power, apparent power, and total PF.

### Cycle and phase reference

The component detects interpolated positive voltage zero crossings and evaluates complete line cycles. Voltage and the
delay-aligned main/multiplexed CT samples are retained in the same cycle ring. A shared sine/cosine reference is then
used for all voltage inputs and all 19 CT channels.

The configured integer current delays compensate the ADC/multiplexer pipeline before the sample enters the common cycle
analysis. They do not claim to correct the individual phase error of every physical CT.

### Fundamental phasors

For each accepted cycle, the component accumulates in-phase and quadrature components:

```text
Xc = sum(x[n] * cos(theta[n]) * weight[n])
Xs = sum(x[n] * sin(theta[n]) * weight[n])
```

Partial boundary samples receive fractional overlap weights. Mains channels and each multiplexed branch channel keep
separate normalization weights. The resulting signed RMS phasors produce:

```text
P1 = Vi * Ii + Vq * Iq
Q1 = Vi * Iq - Vq * Ii
S1 = hypot(Vi, Vq) * hypot(Ii, Iq)
PF1 = abs(P1) / S1
angle = atan2(Q1, P1)
```

For a line-to-line CT, the fundamental voltage phasor is the calibrated vector difference of both configured voltage
phasors before P1, Q1, S1, PF1, and angle are calculated.

### Current THD

The exposed current THD uses total RMS current and fundamental RMS current:

```text
THD_I = sqrt(max(0, I_rms² - I1²)) / I1 * 100%
```

If `I1` is below `minimum_fundamental_current`, the denominator is not trustworthy and THD is `unknown`. If `I1` exceeds
total RMS beyond a small numerical consistency tolerance, THD is also `unknown` instead of incorrectly reporting `0%`.

This residual method is useful for diagnostics, but analog filtering, interharmonics, noise, CT bandwidth, and sample
timing influence the result. Treat it as an experimental waveform indicator, not a standards-compliance report.

### Sign conventions

For normal import with correctly oriented CTs:

- positive fundamental reactive power and positive displacement angle mean lagging/inductive current;
- negative fundamental reactive power and negative displacement angle mean leading/capacitive current;
- fundamental and total power factor are magnitudes from `0` to `1`;
- active power remains the source of import/export direction.

Export and reversed CTs can move P1/Q1 into a different quadrant. For line pairs, the configured order also defines the
reference (`[1, 2]` means `V1 - V2`). Correct the wiring, CT orientation, and line order before interpreting the angle as
a simple inductive/capacitive label. An active-power correction such as `filters: [{ multiply: -1 }]` does not rotate the
fundamental current phasor and therefore does not repair Q1 or the displacement angle.

### Accuracy limits

The algorithms are tested with synthetic 50 Hz and 60 Hz waveforms, phase shifts, harmonic content, every mux alignment,
all CT channels, and every line pair. Real-world absolute accuracy still depends on:

- voltage and current calibration;
- physical CT gain, bandwidth, phase error, and orientation;
- analog anti-alias filtering;
- ADC pipeline delay and sample-clock accuracy;
- load level and waveform shape.

No accuracy class, IEC 61000-4-30 claim, revenue-metering claim, or protection function is implied. Reference-instrument
testing is welcome, especially for low current, motors, inverters, wallboxes, and strongly distorted loads.

## Diagnostics and Maintenance

### Transport diagnostics

Diagnostics are disabled unless `diagnostics_interval` is configured:

```yaml
emporiavue:
  diagnostics_interval: 30s
```

Available entities cover frame errors, transfer errors, overruns, recoveries, last window sample count, measured SPI
sample rate, ESP processing load, processing-queue overruns, free/minimum heap, and task stack reserves. They are intended
for troubleshooting, not normal dashboards. `ESP SPI Processing Load` is the percentage of wall time spent processing
SPI metering frames during the diagnostics interval; a rising `ESP SPI Processing Overruns` counter means complete frames
had to be dropped because the processing queue was full. `ESP SPI Transfer Errors` separately counts receive-queue and
DMA failures; it does not include processing overruns.

### SAMD09 firmware management

ESPHome SPI requires the matching managed SAMD09 firmware. With `auto_update_samd: false`, use these two steps in
Home Assistant:

1. **Back up the original firmware:** press `Read SAMD Firmware` and wait until the ESPHome log reports
   `SAMD09 legacy firmware backup valid`.
2. **Write the managed firmware:** press `Flash SAMD Bundled Firmware` and wait until the ESPHome log reports
   `SAMD09 firmware update complete`. To return to the saved original firmware, press `Flash SAMD Backup Firmware`.

`Read SAMD Firmware` always writes the complete firmware as offset-tagged hexadecimal chunks to the INFO log. When
the `samd_bak` partition is available, the same verified image is also stored there for one-button restoration.

`auto_update_samd` defaults to `false`.

To leave the SAMD09 completely untouched while ESPHome starts, disable the initial SWD firmware detection. This also
skips its associated reset; the manual backup, install, and restore buttons can still open an SWD session when used.
The default is `true`.

```yaml
emporiavue:
  swd_on_boot: false
```

> [!WARNING]
> Flashing changes the measurement-controller firmware. Keep a backup and understand the recovery path. ESPHome
> selects a model-specific image and rejects target mismatches; never manually flash an image built for another model.

The `samd_bak` partition needs 64 KiB. When adding it to an already-flashed ESP32, update the partition table once. OTA
partition-table updates require `allow_partition_access: true`.

```yaml
emporiavue:
  auto_update_samd: false
```

### Optional GPIO helpers

- `packages/vue2-gpios.yaml` controls the Vue 2 GPIO23 Wi-Fi/status LED.
- `packages/vue3-gpios.yaml` provides Vue 3 Wi-Fi and Ethernet status outputs.

These packages are optional and separate from metering.

## Package Reference

| File | Purpose |
|---|---|
| `packages/vue2-i2c.yaml` | Vue 2 stock-compatible I2C transport and firmware management |
| `packages/vue2-spi.yaml` | Raw-sample ESPHome SPI transport for Vue 2 and firmware management |
| `packages/vue2-1phase.yaml` | Vue 2 one-line topology |
| `packages/vue2-2phase.yaml` | Vue 2 two-line/split-phase topology |
| `packages/vue2-3phase.yaml` | Vue 2 three-phase-with-neutral topology |
| `packages/vue2-gpios.yaml` | Optional Vue 2 status LED helper |
| `packages/vue3-i2c.yaml` | Vue 3 I2C transport and firmware management |
| `packages/vue3-spi.yaml` | Raw-sample ESPHome SPI transport for Vue 3 and firmware management |
| `packages/vue3-1phase.yaml` | Vue 3 one-line topology |
| `packages/vue3-2phase.yaml` | Vue 3 two-line/split-phase topology |
| `packages/vue3-3phase.yaml` | Vue 3 three-phase-with-neutral topology |
| `packages/vue3-gpios.yaml` | Optional Vue 3 status GPIO helpers |

## Contributing and Validation

Useful contributions include:

- ESPHome SPI comparisons against a trusted power analyzer;
- CT gain and phase-error measurements;
- low-current/noise-floor results;
- 50 Hz and 60 Hz installations;
- split-phase, three-phase, and line-to-line validation;
- Vue 2 and Vue 3 feedback from additional hardware installations.

## Acknowledgements

- [`emporia-vue-local/esphome`](https://github.com/emporia-vue-local/esphome) for the original local Vue metering work.
- [`gekkehenkie11/emporia-SAMD09`](https://github.com/gekkehenkie11/emporia-SAMD09) for publishing a stock-compatible
  SAMD09 firmware reference.
