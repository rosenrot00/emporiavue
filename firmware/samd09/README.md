# Emporia Vue 2 SAMD09 test firmware

This directory vendors the open-source SAMD09 replacement firmware from:

https://github.com/gekkehenkie11/emporia-SAMD09

Imported source commit:

```text
0baafe6d8812639d14f8f66b03844567f913ddc0
```

The upstream repository currently does not include an explicit LICENSE file. Keep redistribution private/test-only until
the firmware licensing is clarified.

Local build patches are intentionally small:

- remove unused hosted C library includes because the Homebrew `arm-none-eabi-gcc` package does not ship Newlib headers;
- add freestanding fixed-width typedefs plus tiny `memset`/`memcpy`, built with no-builtin flags so GCC cannot rewrite
  those routines into recursive library calls;
- add explicit pointer casts required by current GCC;
- link without startup/default libraries and explicitly link `libgcc`.

The generated ESPHome bundle is produced by:

```sh
python3 tools/package_samd09_firmware.py
```

The generated image is padded to the full 16 KiB SAMD09D14 flash size and ends with the EmporiaVue managed firmware
footer used by the ESPHome component for SWD/offline `hardware_id`, `mode_id`, and `firmware_version` detection.
Runtime identity is reported through the I2C diagnostic command together with the health counters. The current image is
Vue 2 I2C firmware (`hardware_id=2`, `mode_id=1`, `firmware_version=10`, displayed as `v1.0`).
