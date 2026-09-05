# Emporia Vue ESPHome

Turn an Emporia Vue into a local ESPHome energy meter for Home Assistant. Start with dependable ESPHome I2C metering,
or use the ESPHome SPI path when you specifically want synchronized raw-waveform analysis.

## Version History

| Version | Changes |
|---|---|
| 2026.09.1 | **Breaking change:** `energy:` now converts to kWh automatically. Remove `multiply: 0.001` from local energy filters and `filter_defaults.energy`; it is no longer needed. See [Energy](#energy). Existing exact conversion filters are recognized to prevent double scaling. |
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

Both ESPHome paths support **Vue 2 and Vue 3**. I2C and SPI describe how the ESP32 reads the measurement controller
inside the Vue; they do not change how Home Assistant connects to your device.

|  | Emporia stock | ESPHome I2C | ESPHome SPI |
|---|---|---|---|
| **Best for** | Official Emporia experience | Normal daily monitoring | Enthusiasts and development |
| **You get** | Emporia app and cloud | Local Home Assistant entities | Local entities plus waveform detail |
| **Measurements** | Official Emporia feature set | Voltage, current, power, daily energy, demand, groups and import/export | Same core values plus optional reactive power, phase displacement and waveform analysis |
| **Firmware** |  | Stock SAMD09 firmware works on Vue 2 and Vue 3; Vue 2 custom firmware adds line-to-line voltage without a fixed `√3` assumption | Matching custom SAMD09 firmware required |

**Not sure? Start with ESPHome I2C.** Choose [ESPHome SPI](#esphome-spi) when you specifically want its extra analysis.
Stay with Emporia stock if you want to keep using the official Emporia app and cloud.

**On this page:** [Installation](#installation) · [Quick Start](#quick-start) ·
[Everyday Configuration](#everyday-configuration) · [ESPHome SPI](#esphome-spi) ·
[Troubleshooting & Technical Details](#troubleshooting-and-technical-details)

## Installation

**Already running ESPHome?** Continue with the [Quick Start](#quick-start). Keep your existing Wi-Fi, API encryption
and OTA credentials when adapting the example.

**Still running the original Emporia software?** Thanks to the **emporia-vue-local** community for their detailed
[installation guide for Vue 2 and Vue 3](https://emporia-vue-local.github.io/docs/tutorial/intro/).
Follow it for hardware preparation, backing up the ESP32 firmware, and the first ESPHome installation. Then return
here and use **our packages and configuration below**, instead of the guide's configuration example.

Never connect a serial flashing adapter while the Vue is connected to mains electricity. Electrical panel work belongs
to a qualified person; follow the safety instructions in the installation guide.

The Vue contains two controllers: the **ESP32** runs ESPHome and connects to Home Assistant; the **SAMD09** handles
measurement acquisition. I2C can keep the stock SAMD09 firmware. SPI needs an additional
[SAMD09 backup and firmware installation](#set-up-spi), which is separate from the ESP32 backup above.

## Quick Start

The goal is a working local meter with line voltages, Grid power, and one named circuit with power and daily energy.
You can add everything else later.

### 1. Choose your model and electrical layout

Combine one **base package** for your model and transport with one **topology package** for the connected voltage
inputs. Include the matching **GPIO package** for the status LEDs; it is recommended but not required for metering.

| Device | I2C base package | Status LEDs |
|---|---|---|
| Vue 2 | `packages/vue2-i2c.yaml` | `packages/vue2-gpios.yaml` |
| Vue 3 | `packages/vue3-i2c.yaml` | `packages/vue3-gpios.yaml` |

| Connected voltage inputs | Vue 2 topology | Vue 3 topology |
|---|---|---|
| One measured line | `packages/vue2-1phase.yaml` | `packages/vue3-1phase.yaml` |
| Two measured lines / split phase | `packages/vue2-2phase.yaml` | `packages/vue3-2phase.yaml` |
| Three phases with neutral | `packages/vue2-3phase.yaml` | `packages/vue3-3phase.yaml` |

Choose from the actual installation, not from the number of CT clamps you use. A three-phase installation without
neutral needs a [specialized configuration](#three-phase-without-neutral), not the standard three-phase example.

### 2. Start with this configuration

This is a **complete starting configuration for Vue 2, I2C, three phases with neutral**. Change the three package
filenames for your model and layout using the tables above. Adapt the device name and circuit assignment as well.

Three terms matter before copying:

- **`cir1`** means branch CT socket 1. It does not mean electrical line 1.
- **`line: 1`** selects the voltage reference for that CT. All circuits start on line 1; correct this to match your panel.
  If the line is unknown, see [automatic line assignment](#automatic-line-assignment).
- **`power:`** is the present load in watts; **`energy:`** is today's accumulated energy in kWh, resetting at midnight.

```yaml
esphome:
  name: emporiavue2
  friendly_name: Vue 2

esp32:
  board: esp32dev
  framework:
    type: esp-idf
    version: recommended

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
      - packages/vue2-i2c.yaml
      - packages/vue2-3phase.yaml
      - packages/vue2-gpios.yaml

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

ota:
  - platform: esphome
    allow_partition_access: true
    on_begin:
      then:
        - lambda: global_preferences->sync();

logger:
  logs:
    sensor: INFO

preferences:
  # Avoid periodic flash writes; save pending preferences before OTA instead.
  flash_write_interval: never

time:
  - platform: sntp
    id: my_time

emporiavue:
  filter_defaults:
    power:
      - throttle_average: 5s
    energy:
      - throttle: 60s

  circuits:
    cir1:
      name: "Living Room"
      line: 1  # Match the voltage reference for the CT in socket 1.
      power:
      energy:

  groups:
    grid:
      power:
```

The Wi-Fi values belong in your ESPHome `secrets.yaml` file. If they do not already exist, add:

```yaml
wifi_ssid: "Your Wi-Fi name"
wifi_password: "Your Wi-Fi password"
```

Keep `flash_write_interval: never` and the OTA synchronization above. They reduce background flash writes and help
keep saved values consistent. Calibration controls and line selectors save immediately when changed; see
[Saving values and measurement gaps](#saving-values-and-measurement-gaps) for the limits.

`allow_partition_access: true` lets OTA install the packages' SAMD09 backup partition. It does **not** flash the SAMD09;
automatic SAMD09 firmware installation is disabled by default.

### 3. Check your first readings

Install the configuration through ESPHome and add the node through Home Assistant's ESPHome integration if needed.

- Look for the line devices, **Grid**, and **Living Room**. Firmware controls remain on the main Vue device.
- Check that voltage and frequency are plausible for your supply.
- Turn a known appliance on and off normally. The corresponding circuit power should change; a consuming load should
  normally read positive with the correct line assignment and CT orientation.
- **Today's Energy** should accumulate while that circuit consumes power. It requires valid time and resets at midnight.

Incorrect or missing readings? Start with [Common problems](#common-problems), not calibration changes.
Once these basics work, add more circuits and features below.

## Everyday Configuration

These recipes extend the Quick Start; they are **configuration fragments**, not separate node files. Merge them into
the corresponding existing block. For example, put another `cir2:` under your existing `emporiavue: circuits:` rather
than adding a second top-level `emporiavue:` block. Examples beginning with `cir1:` belong inside `circuits:`.

Choose what you need:

- [Add circuits](#add-and-name-circuits) and [assign the right line](#assign-the-right-line).
- [Daily energy](#energy), [groups](#groups), and [Grid import/export](#grid-import-and-export).
- [Organize Home Assistant devices](#home-assistant-devices).
- [Smooth readings](#display-intervals-and-filters) or [track demand](#demand).
- [Detect a line automatically](#automatic-line-assignment) or [show a detection result only](#line-detection-helper).

### Add and name circuits

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

Without `name:`, a circuit keeps its package name, such as `Circuit 1`. Add only the measurements you need;
unused branch inputs do not need visible entities. A Home Assistant entity is an individual sensor or control;
a device groups those entities together. See [Home Assistant devices](#home-assistant-devices) for naming and grouping.

### Assign the right line

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

For example, a CT in socket 3 can use `cir3: { line: 1 }`. Socket numbers and voltage-reference numbers are independent.
For an unknown assignment, use [automatic line assignment](#automatic-line-assignment) or the
[diagnostic-only line detection helper](#line-detection-helper).

### Energy

`energy:` creates **Today's Energy**, not a lifetime total: it accumulates during the day and resets at midnight.
The unit is kWh by default, and a `time:` source is required. For example, a constant 1,000 W load running for one
hour adds 1 kWh. A visible `power:` sensor is not required.

```yaml
cir1:
  name: "Living Room"
  energy:
    # state_class: total  # Uncomment for explicit signed/net energy.
    filters:
      - throttle: 5s  # Optional; energy is already converted to kWh.
```

Simple daily energy defaults to `state_class: total_increasing`. Explicit signed/net energy can use `state_class: total`.
`both`, `positive`, and `negative` are power directions, not energy modes. A nested `energy:` integrates that selected
output: `both` is signed/net, `positive` keeps positive power, and `negative` exposes negative power as a positive value.
An `energy:` directly under the circuit uses `both`. Use separate positive/import and negative/export energy rather than
signed net energy in the Home Assistant Energy Dashboard.

**Breaking change in 2026.09.1:** Energy is converted to `kWh` automatically, before any display filters.
`unit_of_measurement: Wh` or `MWh` also works.
Remove the old `multiply: 0.001` conversion from your energy filters; existing configurations with that exact filter
are handled without applying the conversion twice. Other filters still apply normally. Stored daily totals remain in
the same internal unit and are not reset by this change.
All examples, including the no-neutral setup, use `energy:` directly on their circuits; no separate energy helper is
needed.

See [Saving values and measurement gaps](#saving-values-and-measurement-gaps) for restart and outage behavior.

### Groups

A group combines measurements, for example the three circuits of a heat pump. `sources` names the mains, circuits,
or other groups to add; a leading `-` subtracts a source. No ESPHome template sensors are needed.
The referenced circuits must have correct line assignments; group membership does not assign their lines.

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

Here `unmonitored` is the Grid total minus circuits 1–3. List each measured load only once; do not subtract both a
group and the circuits already included in it. To combine the source entities and totals on one Home Assistant
device as well, see [`sources_to_subdevice`](#home-assistant-devices).

### Grid import and export

The topology package already defines `grid` as a normal group with the appropriate main lines as its `sources`.
You only need to add the outputs you want to see; there is no need to repeat those sources.

```yaml
emporiavue:
  groups:
    grid:
      power:
        both:
        positive:
          energy:
        negative:
          energy:
```

| Output | What it shows | Nested `energy:` |
|---|---|---|
| `both` | Net power: positive import, negative export | Signed/net daily energy, if added |
| `positive` | Power taken from the grid | Today's imported energy |
| `negative` | Power sent to the grid, displayed as a positive value | Today's exported energy |

Use the separate import and export energy entities in the Home Assistant Energy Dashboard, not signed net energy.
The same directional structure also works on an individual circuit.

### Home Assistant devices

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
      line: 1
      power:
      current:

    cir3:
      name: "Heat Pump L2"
      line: 2
      power:
      current:

    cir4:
      name: "Heat Pump L3"
      line: 3
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
emporiavue:
  groups:
    combined_load:
      sources: [cir2, cir3, cir4, cir5]
      sources_to_subdevice: [cir2, cir3, cir4]
```

<details>
<summary>Source selection and naming rules</summary>

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

</details>

### Display intervals and filters

Use `throttle_average` to smooth power, voltage, or current; use `throttle` to publish the latest daily energy less
often. These filters change the display, not the underlying metering or energy integration.

```yaml
emporiavue:
  filter_defaults:
    power:
      - throttle_average: 5s
    voltage:
      - throttle_average: 5s
    current:
      - throttle_average: 5s
    energy:
      - throttle: 60s

  circuits:
    cir1:
      power:
        filters:
          - throttle_average: 10s
```

`filter_defaults` applies only to entities you create; it does not create any by itself. A local `filters:` entry
**replaces**, rather than adds to, that sensor type's global default. In the example, Circuit 1 uses a 10-second power
average; other power entities use 5 seconds. The same mechanism works for optional SPI analysis entities.

For peak readings, choose [`peak_interval`](#circuit-waveform-analysis); for rolling demand, choose
[`demand_interval`](#demand). These define what is measured, rather than only how often it is displayed.
Internal power corrections are different from display filters; see [Metering corrections](#metering-corrections).

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

On the `Heat Pump` subdevice, this creates entities such as `Power Demand` and `Today's Maximum Power Demand`.
The rolling demand does not reset; it always represents the latest complete interval and stays `unknown` until that
first interval is available. The daily maximum resets at midnight and only starts again after a complete interval from
the new day. Like daily energy, its state is restored after a restart by default. The maximum entities need an ESPHome `time:` source.
All Demand entities are optional and work with both I2C and SPI.

### Automatic line assignment

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

The optional `Line` selector on the `Heat Pump` device offers `Auto Import`, `Auto Export`, and every configured line,
for example `L1`, `L2`, and `L3`. It stays on the selected automatic mode until detection is reliable and then changes to the
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

To use it, let the appliance start or stop normally and keep the new state steady for about 30 seconds.
A result of **`L3` means logical line 3**: set that circuit to `line: 3` yourself. `L3 weak` is only preliminary;
`waiting for change` means it still needs a clear load transition. The last confirmed result may remain visible
even after the load drops. This diagnostic never rewrites your assignment.

<details>
<summary>Directions, thresholds, and detection behavior</summary>

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

</details>

### Line-to-line and three-phase loads

For a load connected between two lines, use a pair:

```yaml
cir8:
  name: "Line-to-line Load"
  line: [1, 2]
  power:
  power_apparent:
  power_factor:
```

A single CT with `line: [1, 2]` represents a two-wire line-to-line load, not an entire three-phase appliance.
For a three-phase appliance, measure each conductor with its own correctly assigned CT and combine the circuits in a
[group](#groups). See the [complete SPI example](#a-complete-spi-example) for a three-phase Wallbox and
[Line-to-line voltage and power](#line-to-line-voltage-and-power) for the calculation.

## ESPHome SPI

Use this path when you want more than everyday power and energy. It is available on both Vue 2 and Vue 3 and requires
the matching custom SAMD09 firmware. The extra analysis entities are optional; you do not have to enable them all.

| What you want to see | Add this | Where |
|---|---|---|
| How distorted the voltage is | `voltage_thd:` | Under a main line |
| Current at the supply's fundamental frequency | `fundamental_current:` | Under a circuit or main line |
| Inductive/capacitive behavior during import | `fundamental_reactive_power:` and `displacement_angle:` | Under a circuit or main line |
| Fundamental power factor | `fundamental_power_factor:` | Under a circuit or main line |
| Estimated current distortion | `current_thd:` | Under a circuit or main line |
| Sampled current peaks and waveform shape | `current_peak:` and `current_crest_factor:` | Under a circuit or main line |

SPI also calculates line-to-line RMS voltage directly from synchronized waveforms. See
[How the SPI analysis works](#how-the-spi-analysis-works) for formulas and accuracy limits.

### Set up SPI

Start with the Quick Start configuration and replace **only the base package**:

| Device | Replace | With |
|---|---|---|
| Vue 2 | `packages/vue2-i2c.yaml` | `packages/vue2-spi.yaml` |
| Vue 3 | `packages/vue3-i2c.yaml` | `packages/vue3-spi.yaml` |

Keep the matching topology and GPIO packages, then install the ESPHome configuration. Stock SAMD09 firmware cannot
provide SPI readings: until the next steps are complete, missing measurements or a firmware-mode mismatch are expected.
The packages expose these buttons on the main Vue device in Home Assistant:

1. **Back up the SAMD09:** open the ESPHome log, press `Read SAMD Firmware`, and wait for
   `SAMD09 legacy firmware backup valid`. Save the complete backup log outside the device before continuing.
2. **Install its SPI firmware:** press `Flash SAMD Bundled Firmware` and wait for `SAMD09 firmware update complete`.
   Check that the running SAMD firmware reports SPI and that live measurements appear.

`auto_update_samd` defaults to `false`, so changing the ESPHome package does not automatically replace the SAMD09
firmware. Use only the image for your Vue model. The SAMD09 backup is separate from the ESP32 factory backup made
during initial installation. For restoration and partition details, see [SAMD09 firmware management](#samd09-firmware-management).

### Voltage THD

Voltage THD describes distortion relative to the fundamental voltage, as a percentage. Add it to the desired **main
voltage references**, not to circuits:

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

Each selected line gets a `Voltage THD` entity. Omit lines that your topology does not use. A local sensor `filters:`
entry replaces the global default, just as for power and energy.

Only requested voltage inputs perform harmonic processing. The display filter averages the results; it does **not**
reduce the underlying calculation rate. When the fundamental voltage is unavailable or too small, the result is
`unknown`. See [Voltage THD calculation](#voltage-thd-calculation) for the harmonics, formula, and limitations.

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

On the `Heat Pump` subdevice, this creates:

```text
Fundamental Current
Fundamental Reactive Power
Fundamental Power Factor
Displacement Angle
Current THD
Current Peak
Current Crest Factor
```

With `esphome_subdevices: false`, the circuit name is included in each entity name instead.
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

### A complete SPI example

This **Vue 2, SPI, three-phase-with-neutral** example shows how the optional features fit together: voltage
calibration controls, SPI analysis, a line selector, per-circuit gain/phase controls, and grouped three-phase loads.
It is a reference, not a list of settings you must enable. For Vue 3, use the three matching Vue 3 packages.

<details>
<summary>Show the complete SPI configuration</summary>

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
      - packages/vue2-gpios.yaml

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

</details>

Keep the preference and OTA settings as explained in [Saving values and measurement gaps](#saving-values-and-measurement-gaps).

The Wallbox example uses one CT per phase. Its `sources_to_subdevice` option places the three phase currents and powers
on the same Home Assistant device as the summed power and energy. The Grid example similarly combines the three main
lines with the Grid totals. A single CT with `line: [1, 2]` represents a two-wire line-to-line load, not a complete
three-phase load.

Full and specialized examples are available in [`examples/yaml`](examples/yaml/).

## Troubleshooting and Technical Details

Normal operation does not require the settings below. Start with the symptom, then enable diagnostics if needed.

### Common problems

| What you see | What to check first |
|---|---|
| A circuit is missing in Home Assistant | Add an entity such as `power:` or `energy:`. Defining a circuit alone does not make it visible. |
| Consumption is negative or the power is implausible | Check `line`, CT orientation, and, for a line pair, its order. Do not hide the sign with an absolute-value filter before understanding it. |
| SPI has no readings or reports stock/unknown firmware | Complete [Set up SPI](#set-up-spi); ESPHome and the SAMD09 must use matching transport modes. |
| Line detection says `waiting for change` | Let the appliance change operating state and remain steady for about 30 seconds; see the [helper](#line-detection-helper). |
| Detection stays ambiguous | The transition may be too small or not distinguish the lines reliably. Wait for another clear load change; do not treat a weak result as a confirmed assignment. |
| PF, angle, or THD is `unknown` at low load | This is intentional below the [analysis thresholds](#circuit-waveform-analysis). |
| Current THD is extremely high | A small fundamental current makes the ratio large; noise and short peaks can dominate. Check current, load level, and [accuracy limits](#accuracy-limits). |
| Daily energy does not reset or demand stays unknown | Check the `time:` source. Demand also needs a complete interval before its first value. |
| Readings turn `unknown` after a transport interruption | Missing data is not replaced with the last power value. See [measurement gaps](#saving-values-and-measurement-gaps). |
| SPI error counters keep increasing | Enable [transport diagnostics](#transport-diagnostics) and inspect the error type in the log; processing overruns and transfer errors have different causes. |

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

### Saving values and measurement gaps

Keep `preferences: flash_write_interval: never` in your node YAML. It prevents periodic background preference writes,
reduces flash wear, and helps keep related restored values consistent by saving them at deliberate synchronization
points. Both complete examples synchronize pending preferences before OTA. EmporiaVue calibration inputs and line
selectors also synchronize immediately when changed.

Daily energy and daily demand maxima restore their last saved state by default. This is not continuous power-loss
protection: an unexpected power cut can lose changes made since the last save. Calibration and line choices are keyed
by the logical main or circuit, such as `line_1` or `cir2`, so renaming it or moving its entities to a subdevice does not
reset those settings.

If no valid metering frame arrives for 2 seconds (or three `metering_interval`s, whichever is longer), instantaneous
measurements become unknown; display filters may delay this. Daily energy and daily maxima are retained, but missing
time is not filled with the last known power. On recovery, energy resumes from a new baseline and demand starts a fresh
window. This applies to both I2C and SPI. An SPI window that loses its reference-voltage cycles is discarded and
resynchronized instead of accumulating indefinitely.

### SAMD09 firmware management

For the initial SPI installation, follow [Set up SPI](#set-up-spi). The firmware buttons are on the main Vue device,
not on a circuit subdevice.

`Read SAMD Firmware` always writes the complete firmware as offset-tagged hexadecimal chunks to the INFO log. When
the `samd_bak` partition is available, the same verified image is also stored there for one-button restoration.
Keep an external copy; a backup stored only on the device can be lost when its flash is erased.

To return to the saved original SAMD09 firmware, press `Flash SAMD Backup Firmware`. Use the matching I2C ESPHome
base package again for normal readings with stock firmware. `auto_update_samd` defaults to `false`:

```yaml
emporiavue:
  auto_update_samd: false
```

The `samd_bak` partition needs 64 KiB. When adding it to an already-flashed ESP32, update the partition table once.
The complete examples include `allow_partition_access: true` on the ESPHome OTA platform for this purpose.

To leave the SAMD09 untouched while ESPHome starts, disable its initial SWD firmware detection with `swd_on_boot: false`.
This also skips the associated reset. Manual backup, install, and restore buttons can still open an SWD session when
used. The default is `true`:

```yaml
emporiavue:
  swd_on_boot: false
```

> [!WARNING]
> Flashing changes the measurement-controller firmware. Keep a backup and understand the recovery path. ESPHome
> selects a model-specific image and rejects target mismatches; never manually flash an image built for another model.

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

### Metering corrections

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

### Three phase without neutral

There is no universal no-neutral preset because the voltage reference depends on the installation. Start with
[`examples/yaml/vue2-3phase-no-neutral.yaml`](examples/yaml/vue2-3phase-no-neutral.yaml) only if you understand the
wiring, safety implications, and line-to-line calculation.

### How the SPI analysis works

This reference explains how the optional SPI entities are calculated. It is not needed for the initial setup.

#### Total waveform measurements

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

#### Cycle and phase reference

The component detects interpolated positive voltage zero crossings and evaluates complete line cycles. Voltage and the
delay-aligned main/multiplexed CT samples are retained in the same cycle ring. A shared sine/cosine reference is then
used for all voltage inputs and all 19 CT channels.

The configured integer current delays compensate the ADC/multiplexer pipeline before the sample enters the common cycle
analysis. They do not claim to correct the individual phase error of every physical CT.

#### Fundamental phasors

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

#### Voltage THD calculation

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

#### Current THD

The exposed current THD uses total RMS current and fundamental RMS current:

```text
THD_I = sqrt(max(0, I_rms² - I1²)) / I1 * 100%
```

If `I1` is below `minimum_fundamental_current`, the denominator is not trustworthy and THD is `unknown`. If `I1` exceeds
total RMS beyond a small numerical consistency tolerance, THD is also `unknown` instead of incorrectly reporting `0%`.

This residual method is useful for diagnostics, but analog filtering, interharmonics, noise, CT bandwidth, and sample
timing influence the result. Treat it as an experimental waveform indicator, not a standards-compliance report.

#### Sign conventions

For normal import with correctly oriented CTs:

- positive fundamental reactive power and positive displacement angle mean lagging/inductive current;
- negative fundamental reactive power and negative displacement angle mean leading/capacitive current;
- fundamental and total power factor are magnitudes from `0` to `1`;
- active power remains the source of import/export direction.

Export and reversed CTs can move P1/Q1 into a different quadrant. For line pairs, the configured order also defines the
reference (`[1, 2]` means `V1 - V2`). Correct the wiring, CT orientation, and line order before interpreting the angle as
a simple inductive/capacitive label. An active-power correction such as `filters: [{ multiply: -1 }]` does not rotate the
fundamental current phasor and therefore does not repair Q1 or the displacement angle.

#### Accuracy limits

The algorithms are tested with synthetic 50 Hz and 60 Hz waveforms, phase shifts, harmonic content, every mux alignment,
all CT channels, and every line pair. Real-world absolute accuracy still depends on:

- voltage and current calibration;
- physical CT gain, bandwidth, phase error, and orientation;
- analog anti-alias filtering;
- ADC pipeline delay and sample-clock accuracy;
- load level and waveform shape.

No accuracy class, IEC 61000-4-30 claim, revenue-metering claim, or protection function is implied. Reference-instrument
testing is welcome, especially for low current, motors, inverters, wallboxes, and strongly distorted loads.

### Status LED GPIO helpers

- `packages/vue2-gpios.yaml` controls the Vue 2 GPIO23 Wi-Fi/status LED.
- `packages/vue3-gpios.yaml` provides Vue 3 Wi-Fi and Ethernet status outputs.

The Vue 2 examples above include its GPIO package by default. These helpers remain separate from metering and can be
omitted if you do not want the status LEDs.

### Package reference

| File | Purpose |
|---|---|
| `packages/vue2-i2c.yaml` | Vue 2 stock-compatible I2C transport and firmware management |
| `packages/vue2-spi.yaml` | Raw-sample ESPHome SPI transport for Vue 2 and firmware management |
| `packages/vue2-1phase.yaml` | Vue 2 one-line topology |
| `packages/vue2-2phase.yaml` | Vue 2 two-line/split-phase topology |
| `packages/vue2-3phase.yaml` | Vue 2 three-phase-with-neutral topology |
| `packages/vue2-gpios.yaml` | Recommended Vue 2 status LED helper |
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
