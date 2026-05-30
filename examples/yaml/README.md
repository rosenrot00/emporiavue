# YAML Examples

These examples are copy/paste starting points for common and advanced Emporia Vue 2 I2C setups.

- `vue2-i2c-3phase.yaml`: compact 3phase setup with neutral.
- `vue2-i2c-3phase-no-neutral.yaml`: advanced 3phase setup without neutral in the measured panel, using one line as a pseudo-reference and `line: [1, 3]` for line-to-line power.

Always review line assignments, CT direction, filters, and electrical safety before using an example on real hardware.
