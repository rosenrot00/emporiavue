# EmporiaVue SAMD09 SWD reader

ESPHome external component for the Emporia Vue ESP32. It bit-bangs SWD on the pins from the Emporia Vue local discussion:

- SWDIO: GPIO13
- SWCLK: GPIO14

The SAMD reset line is intentionally not configured by default because public notes and board-level testing do not fully agree on the ESP32 GPIO. Configure it explicitly only when you want a reset pulse. GPIO26 is mentioned in the original discussion; GPIO4 is another candidate to test on some boards.

The first implementation only checks whether the SAMD09 can be read. Pressing the generated Home Assistant button logs:

- the ARM SWD DP IDCODE
- the SAMD DSU DID register
- the DSU/NVM protection flags and whether a flash probe read succeeded

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

The default config creates Home Assistant buttons for probing, reading, dumping, backing up, and preparing a managed
firmware install for the SAMD09:

- `Probe SAMD09 SWD`: reads only the SWD Debug Port IDCODE and logs the raw ACK value. It tries a plain SWD line-reset sequence, the standard 16-bit SWJ JTAG-to-SWD select sequence, and the 32-bit `0xe79e` variant used by odewdney's MicroPython SWD script.
- `Read SAMD09`: runs the fuller SWD read check, including DSU/NVM status reads after the Debug Port responds.
- `Dump SAMD09 Flash Blocks`: reads a small number of flash blocks and logs numbered hex chunks that can later be reassembled.
- `Backup SAMD09 Firmware`: backs up detected legacy SAMD09 firmware into the `samd_bak` ESP32 data partition. It refuses to back up firmware marked as managed by this project.
- `Install SAMD09 Firmware`: checks whether the running SAMD09 firmware is older than the component's required managed
  firmware version and can flash the bundled managed SAMD09 image when writes are explicitly enabled.

You need the normal ESPHome `api:` setup in your node config for Home Assistant to see those buttons. The results appear in the ESPHome log/console at `INFO` level.
If the `samd_bak` partition is not present at boot, the firmware status entity reports `backup partition missing` and
pressing the backup button is ignored. ESPHome does not currently provide a safe runtime API to dynamically disable or
hide a button entity based on partition-table state.

The install check identifies managed firmware through a footer at the end of SAMD flash. Firmware without that footer is
treated as stock/legacy and therefore as version `0`. The current upstream `emporia_vue` I2C frame is 284 bytes; a future
managed SAMD firmware can expose a version in its I2C payload too, but this SWD component does not depend on that yet.
Because Home Assistant buttons cannot be disabled dynamically by an external component, use `SAMD Firmware Update
Available` and `SAMD Firmware Status` as the authoritative state. The install button exits without writing if no update
is needed, SAMD writes are not explicitly enabled, no bundled image is compiled in, or a valid backup is missing.

The bundled SAMD09 image is built from `firmware/samd09`, which is based on
`gekkehenkie11/emporia-SAMD09` at commit `0baafe6d8812639d14f8f66b03844567f913ddc0` with small local build fixes for
a freestanding ARM GCC toolchain. The generated image is padded to the detected 16 KiB SAMD09 flash size and ends with a
managed firmware footer so future runs can detect its version. To rebuild the embedded header after changing the SAMD
source, run:

```bash
python3 tools/package_samd09_firmware.py
```

Flashing is intentionally opt-in. The component erases one SAMD NVM row at a time, writes one page per ESPHome loop
cycle, verifies each page immediately, and leaves the SAMD core halted if a write has started and a later step fails.
That keeps the ESP32 and ESPHome reachable while avoiding a reset into a partially written SAMD image.

By default the SWD pins are not initialized at boot. `init_pins_on_boot` defaults to `false`, so SWDIO/SWCLK are only touched while a SAMD09 button action is running. The optional reset pin is only touched when `reset_before_read: true` or `connect_under_reset: true` is set. After the check, the component releases the touched pins back to input/pullup.

To test a reset-assisted read, set the reset pin explicitly:

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

The flash dump button defaults to five 64-byte blocks starting at flash address `0x00000000`. It does not reset after each block; the component attaches once over SWD, powers up the Debug Port, halts the SAMD09 core, then reads one block per ESPHome `loop()` cycle so other components get scheduler time between blocks. By default the core is resumed after the full dump. Each block is logged as one line:

```text
SAMD09_FLASH_DUMP block=0000 addr=0x00000000 len=64 data=...
```

The initial test size can be changed:

```yaml
emporiavue:
  id: samd_reader
  dump_start_address: 0
  dump_block_size: 64
  dump_block_count: 5
  dump_halt_core: true
  dump_resume_between_blocks: false
```

To dump the full flash in one button action, enable `dump_full_flash`. The component reads `NVMCTRL.PARAM` at
`0x41004008`, computes `flash_size = page_size * page_count`, and derives the required block count from the
configured block size:

```yaml
emporiavue:
  id: samd_reader
  dump_start_address: 0
  dump_block_size: 64
  dump_full_flash: true
  dump_halt_core: true
  dump_resume_between_blocks: false
```

The full dump still runs one block per ESPHome `loop()` cycle. That keeps the ESP32 scheduler responsive, but the
SAMD09 core remains halted until the dump finishes unless `dump_resume_between_blocks` is enabled.

If the SAMD firmware appears to take over the SWD pins before the probe can connect, try connect-under-reset. This keeps reset asserted while the SWD Debug Port IDCODE is probed, then releases reset again:

```yaml
emporiavue:
  id: samd_reader
  reset_pin: GPIO26
  connect_under_reset: true
```

Optional diagnostic entities can be enabled if you later want the values in Home Assistant too:

```yaml
emporiavue:
  id: samd_reader
  swd_idcode:
    name: "SAMD09 SWD IDCODE"
  dsu_did:
    name: "SAMD09 DSU DID"
  read_allowed:
    name: "SAMD09 Read Allowed"
  status:
    name: "SAMD09 SWD Status"
```

## Vue 2 managed package

The repository includes `packages/vue2-managed.yaml`. It configures the Vue 2 internal SWD pins through
`hardware: vue2`, adds a 64 KiB `samd_bak` data partition, and enables the firmware status entity plus the backup
button.

Keep your private `external_components` block in the main node YAML, then include the package:

```yaml
packages:
  emporiavue_vue2:
    url: https://github.com/rosenrot00/emporiavue
    ref: main
    username: !secret github_username
    password: !secret github_token
    files:
      - packages/vue2-managed.yaml
```

When adding `samd_bak` to a device that is already flashed, update the ESP32 partition table once. ESPHome documents
custom partition lists under `esp32.partitions`, and partition-table OTA needs `allow_partition_access: true` on the
ESPHome OTA platform before running `esphome upload --partition-table`.

The managed package keeps SAMD writes disabled by default. After a valid legacy backup exists, enable a test flash in
the main YAML by overriding only this option:

```yaml
emporiavue:
  allow_samd_write: true
```

## Notes

`read_allowed` is `true` only if DSU `STATUSB.PROT` is clear and a read from flash address `0x00000000` succeeds.

## Future SAMD09 firmware improvements

- Add a generic per-CT power calculation mode for line-to-line loads. The `gekkehenkie11/Emporia-VUE-fix`
  firmware changes selected CT ports to compute power against `L1-L2`, `L1-L3`, and `L2-L3` instead of
  only single phase-to-neutral voltage references. That is useful for two-phase/line-to-line consumers,
  but it should be implemented as a configurable mode per CT port rather than as a hardcoded mux range.
