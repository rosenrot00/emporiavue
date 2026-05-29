import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import button, i2c, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_RESET_PIN,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["esp32", "i2c"]
AUTO_LOAD = ["button", "sensor", "text_sensor"]

emporiavue_ns = cg.esphome_ns.namespace("emporiavue")
EmporiaVueComponent = emporiavue_ns.class_(
    "EmporiaVueComponent", cg.Component, i2c.I2CDevice
)
EmporiaVueBackupFirmwareButton = emporiavue_ns.class_(
    "EmporiaVueBackupFirmwareButton", button.Button
)
EmporiaVueInstallFirmwareButton = emporiavue_ns.class_(
    "EmporiaVueInstallFirmwareButton", button.Button
)
EmporiaVueRestoreFirmwareButton = emporiavue_ns.class_(
    "EmporiaVueRestoreFirmwareButton", button.Button
)

CONF_SWCLK_PIN = "swclk_pin"
CONF_SWDIO_PIN = "swdio_pin"
CONF_BACKUP_FIRMWARE_BUTTON = "backup_firmware_button"
CONF_INSTALL_FIRMWARE_BUTTON = "install_firmware_button"
CONF_RESTORE_FIRMWARE_BUTTON = "restore_firmware_button"
CONF_DIAGNOSTICS_STATUS = "diagnostics_status"
CONF_DIAG_SAMPLE_BLOCKS = "diag_sample_blocks"
CONF_DIAG_PACKETS_BUILT = "diag_packets_built"
CONF_DIAG_PACKETS_READ = "diag_packets_read"
CONF_DIAG_DMA_TRANSFER_ERRORS = "diag_dma_transfer_errors"
CONF_DIAG_PACKET_OVERRUNS = "diag_packet_overruns"
CONF_DIAG_I2C_PARTIAL_READS = "diag_i2c_partial_reads"
CONF_DIAG_I2C_OVERSIZE_READS = "diag_i2c_oversize_reads"
CONF_DIAG_LAST_SAMPLE_COUNT = "diag_last_sample_count"
CONF_DIAG_LAST_I2C_READ_LEN = "diag_last_i2c_read_len"
CONF_FIRMWARE_STATUS = "firmware_status"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_BACKUP_PARTITION = "backup_partition"
CONF_HARDWARE = "hardware"
CONF_RESET_BEFORE_READ = "reset_before_read"
CONF_RESET_ON_BOOT = "reset_on_boot"
CONF_CONNECT_UNDER_RESET = "connect_under_reset"
CONF_RESET_HOLD_TIME = "reset_hold_time"
CONF_RESET_RELEASE_TIME = "reset_release_time"
CONF_CLOCK_DELAY = "clock_delay"
CONF_RETRY_COUNT = "retry_count"
CONF_INIT_PINS_ON_BOOT = "init_pins_on_boot"
CONF_MODE = "mode"
CONF_ENTITY_PREFIX = "entity_prefix"
CONF_AUTO_UPDATE_SAMD = "auto_update_samd"

HARDWARE_CUSTOM = "custom"
HARDWARE_VUE2 = "vue2"
HARDWARE_VUE3 = "vue3"
MODE_I2C = "i2c"
MODE_SPI = "spi"

HARDWARE_IDS = {
    HARDWARE_CUSTOM: 0,
    HARDWARE_VUE2: 2,
    HARDWARE_VUE3: 3,
}

MODE_IDS = {
    MODE_I2C: 0,
    MODE_SPI: 1,
}

DEFAULT_ENTITY_NAMES = {
    CONF_FIRMWARE_STATUS: "SAMD Firmware Status",
    CONF_FIRMWARE_VERSION: "SAMD Firmware Version",
    CONF_DIAGNOSTICS_STATUS: "SAMD Diagnostics Status",
    CONF_DIAG_SAMPLE_BLOCKS: "SAMD Sample Blocks",
    CONF_DIAG_PACKETS_BUILT: "SAMD Packets Built",
    CONF_DIAG_PACKETS_READ: "SAMD Packets Read",
    CONF_DIAG_DMA_TRANSFER_ERRORS: "SAMD DMA Transfer Errors",
    CONF_DIAG_PACKET_OVERRUNS: "SAMD Packet Overruns",
    CONF_DIAG_I2C_PARTIAL_READS: "SAMD I2C Partial Reads",
    CONF_DIAG_I2C_OVERSIZE_READS: "SAMD I2C Oversize Reads",
    CONF_DIAG_LAST_SAMPLE_COUNT: "SAMD Last Sample Count",
    CONF_DIAG_LAST_I2C_READ_LEN: "SAMD Last I2C Read Length",
    CONF_BACKUP_FIRMWARE_BUTTON: "Backup SAMD09 Firmware",
    CONF_INSTALL_FIRMWARE_BUTTON: "Update SAMD09 Firmware",
    CONF_RESTORE_FIRMWARE_BUTTON: "Restore SAMD09 Backup Firmware",
}


def _prefixed_entity_name(prefix, name):
    if not prefix:
        return name
    full_prefix = f"{prefix} "
    if name == prefix or name.startswith(full_prefix):
        return name
    return f"{full_prefix}{name}"


def _apply_entity_name_defaults(config):
    config = dict(config)
    prefix = config.get(CONF_ENTITY_PREFIX, "")
    if not prefix:
        return config

    for key, default_name in DEFAULT_ENTITY_NAMES.items():
        entity_config = config.get(key)
        if entity_config is None:
            entity_config = {}
        elif not isinstance(entity_config, dict):
            continue
        else:
            entity_config = dict(entity_config)

        if entity_config.get(CONF_NAME, default_name) == default_name:
            entity_config[CONF_NAME] = _prefixed_entity_name(prefix, default_name)

        config[key] = entity_config
    return config


def _apply_hardware_defaults(config):
    config = dict(config)
    if config.get(CONF_HARDWARE) == HARDWARE_VUE2:
        config.setdefault(CONF_SWDIO_PIN, "GPIO13")
        config.setdefault(CONF_SWCLK_PIN, "GPIO14")
        config.setdefault(CONF_RESET_PIN, "GPIO26")
        config.setdefault(CONF_RESET_BEFORE_READ, True)
        config.setdefault(CONF_RESET_ON_BOOT, True)
        config.setdefault(CONF_CONNECT_UNDER_RESET, True)
        config.setdefault(CONF_RESET_HOLD_TIME, "200ms")
        config.setdefault(CONF_RESET_RELEASE_TIME, "1ms")
        config.setdefault(CONF_CLOCK_DELAY, 2)
        config.setdefault(CONF_RETRY_COUNT, 40)
        config.setdefault(CONF_INIT_PINS_ON_BOOT, False)
    return config


def _apply_defaults(config):
    return _apply_entity_name_defaults(_apply_hardware_defaults(config))


EMPORIAVUE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EmporiaVueComponent),
        cv.Optional(CONF_HARDWARE, default=HARDWARE_CUSTOM): cv.one_of(
            HARDWARE_CUSTOM, HARDWARE_VUE2, HARDWARE_VUE3, lower=True
        ),
        cv.Optional(CONF_SWDIO_PIN, default="GPIO13"): pins.internal_gpio_input_pullup_pin_schema,
        cv.Optional(CONF_SWCLK_PIN, default="GPIO14"): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RESET_BEFORE_READ, default=False): cv.boolean,
        cv.Optional(CONF_RESET_ON_BOOT, default=False): cv.boolean,
        cv.Optional(CONF_CONNECT_UNDER_RESET, default=False): cv.boolean,
        cv.Optional(CONF_RESET_HOLD_TIME, default="100ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RESET_RELEASE_TIME, default="50ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CLOCK_DELAY, default=2): cv.int_range(min=0, max=50),
        cv.Optional(CONF_RETRY_COUNT, default=40): cv.int_range(min=1, max=255),
        cv.Optional(CONF_INIT_PINS_ON_BOOT, default=False): cv.boolean,
        cv.Optional(CONF_MODE, default=MODE_I2C): cv.one_of(MODE_I2C, MODE_SPI, lower=True),
        cv.Optional(CONF_ENTITY_PREFIX): cv.string_strict,
        cv.Optional(CONF_AUTO_UPDATE_SAMD, default=False): cv.boolean,
        cv.Optional(CONF_BACKUP_PARTITION, default="samd_bak"): cv.string_strict,
        cv.Optional(
            CONF_FIRMWARE_STATUS,
            default={CONF_NAME: "SAMD Firmware Status"},
        ): text_sensor.text_sensor_schema(
            icon="mdi:chip",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_FIRMWARE_VERSION,
            default={CONF_NAME: "SAMD Firmware Version"},
        ): text_sensor.text_sensor_schema(
            icon="mdi:chip",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAGNOSTICS_STATUS,
            default={CONF_NAME: "SAMD Diagnostics Status"},
        ): text_sensor.text_sensor_schema(
            icon="mdi:stethoscope",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_SAMPLE_BLOCKS,
            default={CONF_NAME: "SAMD Sample Blocks"},
        ): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_PACKETS_BUILT,
            default={CONF_NAME: "SAMD Packets Built"},
        ): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_PACKETS_READ,
            default={CONF_NAME: "SAMD Packets Read"},
        ): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_DMA_TRANSFER_ERRORS,
            default={CONF_NAME: "SAMD DMA Transfer Errors"},
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_PACKET_OVERRUNS,
            default={CONF_NAME: "SAMD Packet Overruns"},
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_I2C_PARTIAL_READS,
            default={CONF_NAME: "SAMD I2C Partial Reads"},
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_I2C_OVERSIZE_READS,
            default={CONF_NAME: "SAMD I2C Oversize Reads"},
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_LAST_SAMPLE_COUNT,
            default={CONF_NAME: "SAMD Last Sample Count"},
        ): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_LAST_I2C_READ_LEN,
            default={CONF_NAME: "SAMD Last I2C Read Length"},
        ): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_BACKUP_FIRMWARE_BUTTON,
            default={CONF_NAME: "Backup SAMD09 Firmware"},
        ): button.button_schema(
            EmporiaVueBackupFirmwareButton,
            icon="mdi:content-save",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_INSTALL_FIRMWARE_BUTTON,
            default={CONF_NAME: "Update SAMD09 Firmware"},
        ): button.button_schema(
            EmporiaVueInstallFirmwareButton,
            icon="mdi:update",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_RESTORE_FIRMWARE_BUTTON,
            default={CONF_NAME: "Restore SAMD09 Backup Firmware"},
        ): button.button_schema(
            EmporiaVueRestoreFirmwareButton,
            icon="mdi:backup-restore",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x64))

CONFIG_SCHEMA = cv.All(_apply_defaults, EMPORIAVUE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_hardware_id(HARDWARE_IDS[config[CONF_HARDWARE]]))

    swdio_pin = await cg.gpio_pin_expression(config[CONF_SWDIO_PIN])
    cg.add(var.set_swdio_pin(swdio_pin))
    swclk_pin = await cg.gpio_pin_expression(config[CONF_SWCLK_PIN])
    cg.add(var.set_swclk_pin(swclk_pin))
    if reset_pin_config := config.get(CONF_RESET_PIN):
        reset_pin = await cg.gpio_pin_expression(reset_pin_config)
        cg.add(var.set_reset_pin(reset_pin))

    cg.add(var.set_reset_before_read(config[CONF_RESET_BEFORE_READ]))
    cg.add(var.set_reset_on_boot(config[CONF_RESET_ON_BOOT]))
    cg.add(var.set_connect_under_reset(config[CONF_CONNECT_UNDER_RESET]))
    cg.add(var.set_reset_hold_time(config[CONF_RESET_HOLD_TIME]))
    cg.add(var.set_reset_release_time(config[CONF_RESET_RELEASE_TIME]))
    cg.add(var.set_clock_delay_us(config[CONF_CLOCK_DELAY]))
    cg.add(var.set_retry_count(config[CONF_RETRY_COUNT]))
    cg.add(var.set_init_pins_on_boot(config[CONF_INIT_PINS_ON_BOOT]))
    cg.add(var.set_runtime_mode(MODE_IDS[config[CONF_MODE]]))
    cg.add(var.set_entity_prefix(config.get(CONF_ENTITY_PREFIX, "")))
    cg.add(var.set_auto_update_samd(config[CONF_AUTO_UPDATE_SAMD]))
    cg.add(var.set_backup_partition_name(config[CONF_BACKUP_PARTITION]))
    if firmware_status_config := config.get(CONF_FIRMWARE_STATUS):
        sens = await text_sensor.new_text_sensor(firmware_status_config)
        cg.add(var.set_firmware_status_sensor(sens))
    if firmware_version_config := config.get(CONF_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(firmware_version_config)
        cg.add(var.set_firmware_version_sensor(sens))
    if diagnostics_status_config := config.get(CONF_DIAGNOSTICS_STATUS):
        sens = await text_sensor.new_text_sensor(diagnostics_status_config)
        cg.add(var.set_diagnostics_status_sensor(sens))
    if diag_sample_blocks_config := config.get(CONF_DIAG_SAMPLE_BLOCKS):
        sens = await sensor.new_sensor(diag_sample_blocks_config)
        cg.add(var.set_diag_sample_blocks_sensor(sens))
    if diag_packets_built_config := config.get(CONF_DIAG_PACKETS_BUILT):
        sens = await sensor.new_sensor(diag_packets_built_config)
        cg.add(var.set_diag_packets_built_sensor(sens))
    if diag_packets_read_config := config.get(CONF_DIAG_PACKETS_READ):
        sens = await sensor.new_sensor(diag_packets_read_config)
        cg.add(var.set_diag_packets_read_sensor(sens))
    if diag_dma_transfer_errors_config := config.get(CONF_DIAG_DMA_TRANSFER_ERRORS):
        sens = await sensor.new_sensor(diag_dma_transfer_errors_config)
        cg.add(var.set_diag_dma_transfer_errors_sensor(sens))
    if diag_packet_overruns_config := config.get(CONF_DIAG_PACKET_OVERRUNS):
        sens = await sensor.new_sensor(diag_packet_overruns_config)
        cg.add(var.set_diag_packet_overruns_sensor(sens))
    if diag_i2c_partial_reads_config := config.get(CONF_DIAG_I2C_PARTIAL_READS):
        sens = await sensor.new_sensor(diag_i2c_partial_reads_config)
        cg.add(var.set_diag_i2c_partial_reads_sensor(sens))
    if diag_i2c_oversize_reads_config := config.get(CONF_DIAG_I2C_OVERSIZE_READS):
        sens = await sensor.new_sensor(diag_i2c_oversize_reads_config)
        cg.add(var.set_diag_i2c_oversize_reads_sensor(sens))
    if diag_last_sample_count_config := config.get(CONF_DIAG_LAST_SAMPLE_COUNT):
        sens = await sensor.new_sensor(diag_last_sample_count_config)
        cg.add(var.set_diag_last_sample_count_sensor(sens))
    if diag_last_i2c_read_len_config := config.get(CONF_DIAG_LAST_I2C_READ_LEN):
        sens = await sensor.new_sensor(diag_last_i2c_read_len_config)
        cg.add(var.set_diag_last_i2c_read_len_sensor(sens))
    if backup_firmware_button_config := config.get(CONF_BACKUP_FIRMWARE_BUTTON):
        btn = await button.new_button(backup_firmware_button_config)
        await cg.register_parented(btn, var)
    if install_firmware_button_config := config.get(CONF_INSTALL_FIRMWARE_BUTTON):
        btn = await button.new_button(install_firmware_button_config)
        await cg.register_parented(btn, var)
    if restore_firmware_button_config := config.get(CONF_RESTORE_FIRMWARE_BUTTON):
        btn = await button.new_button(restore_firmware_button_config)
        await cg.register_parented(btn, var)
