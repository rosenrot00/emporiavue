# YAML Examples

These examples are copy/paste starting points for common and advanced Emporia Vue 2 setups.
Use `vue2-i2c.yaml` or `vue2-spi.yaml` as the base package, then add one topology package.

- `vue2-3phase.yaml`: compact 3phase setup with neutral.
- `vue2-3phase-no-neutral.yaml`: advanced 3phase setup without neutral in the measured panel, using one measured line as the Vue voltage reference and `line: [1, 3]` for line-to-line power.

Always review line assignments, CT direction, filters, and electrical safety before using an example on real hardware.
