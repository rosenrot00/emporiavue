# EmporiaVue SAMD09 firmware manager

ESPHome external component for backing up and updating the SAMD09 firmware in Emporia Vue devices. It bit-bangs SWD on the pins from the Emporia Vue local discussion:

- SWDIO: GPIO13
- SWCLK: GPIO14

The SAMD reset line is intentionally not configured by default because public notes and board-level testing do not fully agree on the ESP32 GPIO. Configure it explicitly only when you want a reset pulse. GPIO26 is mentioned in the original discussion; GPIO4 is another candidate to test on some boards.

## Use

Add this repository directory as a local external component source:

```yaml
external_components:
  - source:
      type: local
      path: ./components
    components: [emporiavue]

emporiavue:
  id: samd_reader
```

Or use the private GitHub repository from a machine that has access to it:

```yaml
external_components:
  - source: github://rosenrot00/emporiavue@main
    components: [emporiavue]

emporiavue:
  id: samd_reader
```

If ESPHome runs somewhere that is not authenticated to your private GitHub account, use a GitHub token in `secrets.yaml`:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/rosenrot00/emporiavue.git
      ref: main
      username: !secret github_username
      password: !secret github_token
    components: [emporiavue]
```

The default config creates only the SAMD firmware controls that are needed during normal use:

- `Read SAMD Firmware`: backs up detected legacy SAMD09 firmware into the `samd_bak` ESP32 data partition. It refuses to back up firmware marked as managed by this project.
- `Flash SAMD Bundled Firmware`: flashes the bundled managed SAMD09 image.
- `Flash SAMD Backup Firmware`: restores the verified firmware image from the `samd_bak` partition.
- `Flash SAMD External Firmware`: appears when `external_samd_firmware` is configured. ESPHome downloads raw images during code generation, embeds them in the ESP32 build, and flashes them without requiring a managed footer or SHA metadata. Images shorter than the detected SAMD flash are padded with `0xFF`; images larger than the detected flash are rejected.

Example single external image configuration:

```yaml
emporiavue:
  external_samd_firmware:
    url: "https://example.com/samd09.bin"
```

Multiple external images use one entry per firmware. The `id` is used to build the default button name; `button.name`
is optional. Button entities are automatically in the `config` category.

```yaml
emporiavue:
  external_samd_firmware:
    - id: stock
      url: "https://example.com/samd09-stock.bin"
    - id: test
      url: "https://example.com/samd09-test.bin"
      button:
        name: "Flash SAMD Test Firmware"
```

Private GitHub raw URLs can use a token without putting credentials into the URL. The token is sent as an
`Authorization` header during ESPHome code generation:

```yaml
emporiavue:
  external_samd_firmware:
    - id: stock
      url: "https://raw.githubusercontent.com/rosenrot00/emporiavue/main/firmware/images/i2c/vue2-stock-dump.bin"
      token: !secret github_token
```

You need the normal ESPHome `api:` setup in your node config for Home Assistant to see those buttons. The results appear in the ESPHome log/console at `INFO` level.
Default entity names intentionally do not include the device name; ESPHome/Home Assistant use `esphome.friendly_name`
for that prefix when needed. Set `entity_prefix` only when you need this component to write an explicit legacy prefix
into its generated entity names. Set an individual entity `name:` if you need an exact custom name.
If the `samd_bak` partition is not present, pressing the read or backup-flash buttons logs the partition error and
exits without writing. ESPHome does not currently provide a safe runtime API to dynamically disable or hide a button
entity based on partition-table state.

The install check identifies managed firmware through a footer at the end of SAMD flash. Firmware without that footer is
treated as stock/legacy and therefore as `hardware_id=0`, `mode_id=0`, `firmware_version=0`. Managed firmware uses
`hardware_id`, `mode_id`, and `firmware_version` as compatibility keys; the bundled Vue 2 I2C image is
`hardware_id=2`, `mode_id=1`. Internally the version is a monotonic integer in tenths, so `16` is shown as `v1.6`
and `100` as `v10.0`; comparisons and update decisions compare the detected raw integer against the bundled image's raw
integer. The current upstream `emporia_vue` I2C frame is 284 bytes. Managed firmware also returns `hardware_id`,
`firmware_version`, and frame length through the same I2C diagnostic command that reports runtime counters. The SWD flash
footer remains separate on purpose, so the ESP32 can still identify managed firmware when I2C is unavailable.
By default the component only reads this SWD footer at boot and updates the firmware version entities. Set
`auto_update_samd: true` to let the component automatically install the bundled managed image when the SAMD is still on
stock firmware, when a matching managed firmware is older than the bundled image, or when the detected `mode_id` differs
from the configured `mode:`. The selected bundled image must match both the configured hardware and the configured
`mode:`; the automatic path does not overwrite a managed image with a different hardware id.
Because Home Assistant buttons cannot be disabled dynamically by an external component, the flash buttons exit without
writing if their action is not applicable, no bundled/external image is compiled in, or no valid backup is present.

The bundled SAMD09 image is built from `firmware/samd09`, which is based on
`gekkehenkie11/emporia-SAMD09` at commit `0baafe6d8812639d14f8f66b03844567f913ddc0` with small local build fixes for
a freestanding ARM GCC toolchain. The generated image is padded to the detected 16 KiB SAMD09 flash size and ends with a
managed firmware footer so future runs can detect its target hardware, mode, and firmware version. The update path
refuses to flash an image whose `hardware_id` or `mode_id` does not match the configured `hardware:` and `mode:`
values. To rebuild the embedded header after changing the SAMD source, run:

```bash
python3 tools/package_samd09_firmware.py
```

Flashing is intentionally opt-in. The component erases one SAMD NVM row at a time, writes one page per ESPHome loop
cycle, verifies each page immediately, and leaves the SAMD core halted if a write has started and a later step fails.
That keeps the ESP32 and ESPHome reachable while avoiding a reset into a partially written SAMD image.

### Managed SAMD09 changes from stock

- The managed firmware keeps the stock-compatible 284-byte I2C measurement frame, but exposes one diagnostic I2C command
  for runtime identity (`hardware_id`, `firmware_version`, frame length) and health counters before returning to the
  normal frame stream. The SWD footer additionally carries `mode_id`, so the ESP32 can select I2C or SPI-targeted
  SAMD firmware based on the component `mode:`.
- Raw ADC offset correction is intentionally changed from the stock-like direct per-window average replacement to a
  smoothed per-channel DC offset tracker. The tracker runs on all 22 raw ADC channels before RMS, power, phase, and
  frequency are calculated, so it reduces baseline jitter without clamping or filtering finished watt values.
- RMS scaling is calculated with exact integer comparisons instead of the earlier float `sqrtf` path. This removes the
  software floating-point runtime from the SAMD image while preserving the mathematical `floor(sqrt(sum * scale / n))`
  result used for the published frame values.
- Divisions by the fixed sampling windows (`12987` and `1623`) use small exact constant-divisor routines instead of the
  generic 32/64-bit division runtime. Variable divisions, such as cycle-count averaging, are left unchanged.
- Lookup tables for mux ordering, mux output pins, and packet checksum are stored in flash as constants instead of being
  copied into SAMD RAM at startup.

By default the SWD pins are not initialized at boot. `init_pins_on_boot` defaults to `false`, so SWDIO/SWCLK are only touched while a SAMD09 button action is running. The optional reset pin is only touched when `reset_before_read: true` or `connect_under_reset: true` is set. After the check, the component releases the touched pins back to input/pullup.

To test a reset-assisted update or backup, set the reset pin explicitly:

```yaml
emporiavue:
  id: samd_reader
  reset_pin: GPIO26
  reset_before_read: true
  reset_hold_time: 300ms
  reset_release_time: 1ms
```

If you want the ESP32 to recover the SAMD09 after a reboot, enable `reset_on_boot`. This only touches the configured
reset pin during ESPHome setup, holds reset for `reset_hold_time`, releases it for `reset_release_time`, then returns
the reset pin to input/pullup:

```yaml
emporiavue:
  id: samd_reader
  reset_pin: GPIO26
  reset_on_boot: true
  reset_hold_time: 200ms
  reset_release_time: 1ms
```

If the SAMD firmware appears to take over the SWD pins before the probe can connect, try connect-under-reset. This keeps reset asserted while the SWD Debug Port IDCODE is probed, then releases reset again:

```yaml
emporiavue:
  id: samd_reader
  reset_pin: GPIO26
  connect_under_reset: true
```

## Vue 2 I2C Packages

The repository currently includes `packages/vue2-i2c.yaml` and the explicit
`packages/vue2-i2c-three-phase.yaml` preset. They set `hardware: vue2` and `mode: i2c`, add a 64 KiB `samd_bak`
data partition, and enable the firmware version entities plus the backup, update, and restore buttons. They also
enable a slow `metering_interval: 10s` I2C read path that decodes the stock-compatible frame into the component's
internal metering frame without replacing the existing `emporia_vue` sensors. The transport is explicit in the filename
so a future SPI transport can live next to it as `packages/vue2-spi.yaml`.

The three-phase preset creates Home Assistant configuration numbers for the main voltage calibration values. The initial
value is `0.022`, matching the old `emporia_vue` component's documented starting point. If a number was changed before,
the restored ESPHome preference wins over the package value on boot.

Keep your private `external_components` block in the main node YAML, then include the package:

```yaml
packages:
  emporiavue:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    username: !secret github_username
    password: !secret github_token
    files:
      - packages/vue2-i2c-three-phase.yaml
```

When adding `samd_bak` to a device that is already flashed, update the ESP32 partition table once. ESPHome documents
custom partition lists under `esp32.partitions`, and partition-table OTA needs `allow_partition_access: true` on the
ESPHome OTA platform before running `esphome upload --partition-table`.

SAMD writes are enabled by default, and updating the managed SAMD firmware does not require a legacy backup.

Optional metering sensors can be attached to the new transport-independent frame. Keep the interval slow while the
legacy sensor component is still polling the same I2C device:

```yaml
emporiavue:
  metering_interval: 10s
  mains:
    phase_a:
      id: vue_metering_phase_a
      input: BLACK
      clamp: A
      calibration: 0.022
      voltage:
        name: "Vue2 Metering Phase A Voltage"
      frequency:
        name: "Vue2 Metering Phase A Frequency"
    phase_b:
      input: RED
      clamp: B
      calibration: 0.022
      phase_angle:
        name: "Vue2 Metering Phase B Angle"
    phase_c:
      input: BLUE
      clamp: C
      calibration: 0.022
  ct_clamps:
    - phase_id: vue_metering_phase_a
      input: "1"
      power:
        name: "Vue2 Metering Circuit 1 Power"
```

## Future SAMD09 firmware improvements

- Add a generic per-CT power calculation mode for line-to-line loads. The `gekkehenkie11/Emporia-VUE-fix`
  firmware changes selected CT ports to compute power against `L1-L2`, `L1-L3`, and `L2-L3` instead of
  only single phase-to-neutral voltage references. That is useful for two-phase/line-to-line consumers,
  but it should be implemented as a configurable mode per CT port rather than as a hardcoded mux range.
