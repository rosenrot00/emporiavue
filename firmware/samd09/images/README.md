# SAMD09 firmware images

Raw SAMD09 images for flashing through the ESPHome `emporiavue` component.

- `i2c/`: images that use the stock-compatible I2C transport.
- `spi/`: images that use the experimental SPI transport.

The managed firmware packager writes versioned build artifacts here, using names such as `vue2-i2c-v1.0.bin`.
Use raw GitHub URLs from a fixed commit or tag when you want reproducible tests.
