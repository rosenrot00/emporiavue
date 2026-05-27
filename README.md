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

The default config creates two Home Assistant buttons:

- `Probe SAMD09 SWD`: reads only the SWD Debug Port IDCODE and logs the raw ACK value. It tries a plain SWD line-reset sequence, the standard 16-bit SWJ JTAG-to-SWD select sequence, and the 32-bit `0xe79e` variant used by odewdney's MicroPython SWD script.
- `Read SAMD09`: runs the fuller SWD read check, including DSU/NVM status reads after the Debug Port responds.

When `reset_before_read: true` is set, each SWD probe variant gets a fresh reset pulse immediately before it starts. This keeps the timing close to odewdney's MicroPython script, where SWD starts directly after releasing reset.

You need the normal ESPHome `api:` setup in your node config for Home Assistant to see those buttons. The results appear in the ESPHome log/console at `INFO` level.

By default the SWD pins are not initialized at boot. `init_pins_on_boot` defaults to `false`, so SWDIO/SWCLK are only touched while a SAMD09 button action is running. The optional reset pin is only touched when `reset_before_read: true` or `connect_under_reset: true` is set. After the check, the component releases the touched pins back to input/pullup.

To test a reset-assisted read, set the reset pin explicitly:

```yaml
emporiavue:
  id: samd_reader
  reset_pin: GPIO26
  reset_before_read: true
  reset_hold_time: 300ms
  reset_release_time: 1ms
  clock_delay: 2
```

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

## Notes

`read_allowed` is `true` only if DSU `STATUSB.PROT` is clear and a read from flash address `0x00000000` succeeds. The component does not dump flash yet.
