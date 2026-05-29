import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor, button, i2c, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_RESET_PIN,
    CONF_STATUS,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    ICON_DATABASE,
)

DEPENDENCIES = ["esp32", "i2c"]
AUTO_LOAD = ["binary_sensor", "button", "sensor", "text_sensor"]

emporiavue_ns = cg.esphome_ns.namespace("emporiavue")
EmporiaVueComponent = emporiavue_ns.class_(
    "EmporiaVueComponent", cg.Component, i2c.I2CDevice
)
EmporiaVueReadButton = emporiavue_ns.class_(
    "EmporiaVueReadButton", button.Button
)
EmporiaVueProbeButton = emporiavue_ns.class_(
    "EmporiaVueProbeButton", button.Button
)
EmporiaVueDumpFlashButton = emporiavue_ns.class_(
    "EmporiaVueDumpFlashButton", button.Button
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
EmporiaVueFlashDumpFirmwareButton = emporiavue_ns.class_(
    "EmporiaVueFlashDumpFirmwareButton", button.Button
)
EmporiaVueTestWriteButton = emporiavue_ns.class_(
    "EmporiaVueTestWriteButton", button.Button
)
EmporiaVueRuntimeDiagnosticButton = emporiavue_ns.class_(
    "EmporiaVueRuntimeDiagnosticButton", button.Button
)

CONF_SWCLK_PIN = "swclk_pin"
CONF_SWDIO_PIN = "swdio_pin"
CONF_READ_BUTTON = "read_button"
CONF_PROBE_BUTTON = "probe_button"
CONF_DUMP_FLASH_BUTTON = "dump_flash_button"
CONF_BACKUP_FIRMWARE_BUTTON = "backup_firmware_button"
CONF_INSTALL_FIRMWARE_BUTTON = "install_firmware_button"
CONF_RESTORE_FIRMWARE_BUTTON = "restore_firmware_button"
CONF_FLASH_DUMP_FIRMWARE_BUTTON = "flash_dump_firmware_button"
CONF_TEST_WRITE_BUTTON = "test_write_button"
CONF_RUNTIME_DIAGNOSTIC_BUTTON = "runtime_diagnostic_button"
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
CONF_FIRMWARE_ACTION = "firmware_action"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_FIRMWARE_UPDATE_AVAILABLE = "firmware_update_available"
CONF_FIRMWARE_RESTORE_AVAILABLE = "firmware_restore_available"
CONF_BACKUP_PARTITION = "backup_partition"
CONF_ALLOW_SAMD_WRITE = "allow_samd_write"
CONF_REQUIRE_BACKUP_BEFORE_INSTALL = "require_backup_before_install"
CONF_HARDWARE = "hardware"
CONF_SWD_IDCODE = "swd_idcode"
CONF_DSU_DID = "dsu_did"
CONF_READ_ALLOWED = "read_allowed"
CONF_DUMP_START_ADDRESS = "dump_start_address"
CONF_DUMP_BLOCK_SIZE = "dump_block_size"
CONF_DUMP_BLOCK_COUNT = "dump_block_count"
CONF_DUMP_FULL_FLASH = "dump_full_flash"
CONF_DUMP_HALT_CORE = "dump_halt_core"
CONF_DUMP_RESUME_BETWEEN_BLOCKS = "dump_resume_between_blocks"
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
        cv.Optional(CONF_ENTITY_PREFIX, default=""): cv.string_strict,
        cv.Optional(CONF_BACKUP_PARTITION, default="samd_bak"): cv.string_strict,
        cv.Optional(CONF_ALLOW_SAMD_WRITE, default=True): cv.boolean,
        cv.Optional(CONF_REQUIRE_BACKUP_BEFORE_INSTALL, default=False): cv.boolean,
        cv.Optional(CONF_DUMP_START_ADDRESS, default=0): cv.int_range(min=0, max=0xFFFFFFFF),
        cv.Optional(CONF_DUMP_BLOCK_SIZE, default=64): cv.int_range(min=1, max=128),
        cv.Optional(CONF_DUMP_BLOCK_COUNT, default=5): cv.int_range(min=1, max=4096),
        cv.Optional(CONF_DUMP_FULL_FLASH, default=False): cv.boolean,
        cv.Optional(CONF_DUMP_HALT_CORE, default=True): cv.boolean,
        cv.Optional(CONF_DUMP_RESUME_BETWEEN_BLOCKS, default=False): cv.boolean,
        cv.Optional(
            CONF_SWD_IDCODE,
        ): text_sensor.text_sensor_schema(
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DSU_DID,
        ): text_sensor.text_sensor_schema(
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_STATUS,
        ): text_sensor.text_sensor_schema(
            icon=ICON_DATABASE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_FIRMWARE_STATUS,
            default={CONF_NAME: "SAMD Firmware Status"},
        ): text_sensor.text_sensor_schema(
            icon="mdi:chip",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_FIRMWARE_ACTION,
            default={CONF_NAME: "SAMD Firmware Action"},
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
            CONF_FIRMWARE_UPDATE_AVAILABLE,
            default={CONF_NAME: "SAMD Firmware Update Available"},
        ): binary_sensor.binary_sensor_schema(
            icon="mdi:update",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_FIRMWARE_RESTORE_AVAILABLE,
            default={CONF_NAME: "SAMD Stock Restore Available"},
        ): binary_sensor.binary_sensor_schema(
            icon="mdi:backup-restore",
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
            CONF_READ_ALLOWED,
        ): binary_sensor.binary_sensor_schema(
            icon="mdi:database-check",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_READ_BUTTON,
            default={CONF_NAME: "Read SAMD09"},
        ): button.button_schema(
            EmporiaVueReadButton,
            icon=ICON_DATABASE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_PROBE_BUTTON,
            default={CONF_NAME: "Probe SAMD09 SWD"},
        ): button.button_schema(
            EmporiaVueProbeButton,
            icon=ICON_CHIP,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DUMP_FLASH_BUTTON,
            default={CONF_NAME: "Dump SAMD09 Flash Blocks"},
        ): button.button_schema(
            EmporiaVueDumpFlashButton,
            icon=ICON_DATABASE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_BACKUP_FIRMWARE_BUTTON,
            default={CONF_NAME: "Backup SAMD09 Firmware"},
        ): button.button_schema(
            EmporiaVueBackupFirmwareButton,
            icon="mdi:content-save",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
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
            default={CONF_NAME: "Restore Stock SAMD09 Firmware"},
        ): button.button_schema(
            EmporiaVueRestoreFirmwareButton,
            icon="mdi:backup-restore",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_FLASH_DUMP_FIRMWARE_BUTTON,
            default={CONF_NAME: "Flash Dumped SAMD09 Firmware"},
        ): button.button_schema(
            EmporiaVueFlashDumpFirmwareButton,
            icon="mdi:chip",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_TEST_WRITE_BUTTON,
            default={CONF_NAME: "Test SAMD09 Flash Write"},
        ): button.button_schema(
            EmporiaVueTestWriteButton,
            icon="mdi:pencil-check",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_RUNTIME_DIAGNOSTIC_BUTTON,
            default={CONF_NAME: "Diagnose SAMD09 Runtime"},
        ): button.button_schema(
            EmporiaVueRuntimeDiagnosticButton,
            icon="mdi:stethoscope",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x64))

CONFIG_SCHEMA = cv.All(_apply_hardware_defaults, EMPORIAVUE_SCHEMA)


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
    cg.add(var.set_entity_prefix(config[CONF_ENTITY_PREFIX]))
    cg.add(var.set_dump_start_address(config[CONF_DUMP_START_ADDRESS]))
    cg.add(var.set_dump_block_size(config[CONF_DUMP_BLOCK_SIZE]))
    cg.add(var.set_dump_block_count(config[CONF_DUMP_BLOCK_COUNT]))
    cg.add(var.set_dump_full_flash(config[CONF_DUMP_FULL_FLASH]))
    cg.add(var.set_dump_halt_core(config[CONF_DUMP_HALT_CORE]))
    cg.add(var.set_dump_resume_between_blocks(config[CONF_DUMP_RESUME_BETWEEN_BLOCKS]))
    cg.add(var.set_backup_partition_name(config[CONF_BACKUP_PARTITION]))
    cg.add(var.set_allow_samd_write(config[CONF_ALLOW_SAMD_WRITE]))
    cg.add(var.set_require_backup_before_install(config[CONF_REQUIRE_BACKUP_BEFORE_INSTALL]))

    if swd_idcode_config := config.get(CONF_SWD_IDCODE):
        sens = await text_sensor.new_text_sensor(swd_idcode_config)
        cg.add(var.set_swd_idcode_sensor(sens))
    if dsu_did_config := config.get(CONF_DSU_DID):
        sens = await text_sensor.new_text_sensor(dsu_did_config)
        cg.add(var.set_dsu_did_sensor(sens))
    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status_sensor(sens))
    if firmware_status_config := config.get(CONF_FIRMWARE_STATUS):
        sens = await text_sensor.new_text_sensor(firmware_status_config)
        cg.add(var.set_firmware_status_sensor(sens))
    if firmware_action_config := config.get(CONF_FIRMWARE_ACTION):
        sens = await text_sensor.new_text_sensor(firmware_action_config)
        cg.add(var.set_firmware_action_sensor(sens))
    if firmware_version_config := config.get(CONF_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(firmware_version_config)
        cg.add(var.set_firmware_version_sensor(sens))
    if read_allowed_config := config.get(CONF_READ_ALLOWED):
        sens = await binary_sensor.new_binary_sensor(read_allowed_config)
        cg.add(var.set_read_allowed_sensor(sens))
    if firmware_update_available_config := config.get(CONF_FIRMWARE_UPDATE_AVAILABLE):
        sens = await binary_sensor.new_binary_sensor(firmware_update_available_config)
        cg.add(var.set_firmware_update_available_sensor(sens))
    if firmware_restore_available_config := config.get(CONF_FIRMWARE_RESTORE_AVAILABLE):
        sens = await binary_sensor.new_binary_sensor(firmware_restore_available_config)
        cg.add(var.set_firmware_restore_available_sensor(sens))
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
    if read_button_config := config.get(CONF_READ_BUTTON):
        btn = await button.new_button(read_button_config)
        await cg.register_parented(btn, var)
    if probe_button_config := config.get(CONF_PROBE_BUTTON):
        btn = await button.new_button(probe_button_config)
        await cg.register_parented(btn, var)
    if dump_flash_button_config := config.get(CONF_DUMP_FLASH_BUTTON):
        btn = await button.new_button(dump_flash_button_config)
        await cg.register_parented(btn, var)
    if backup_firmware_button_config := config.get(CONF_BACKUP_FIRMWARE_BUTTON):
        btn = await button.new_button(backup_firmware_button_config)
        await cg.register_parented(btn, var)
    if install_firmware_button_config := config.get(CONF_INSTALL_FIRMWARE_BUTTON):
        btn = await button.new_button(install_firmware_button_config)
        await cg.register_parented(btn, var)
    if restore_firmware_button_config := config.get(CONF_RESTORE_FIRMWARE_BUTTON):
        btn = await button.new_button(restore_firmware_button_config)
        await cg.register_parented(btn, var)
    if flash_dump_firmware_button_config := config.get(CONF_FLASH_DUMP_FIRMWARE_BUTTON):
        btn = await button.new_button(flash_dump_firmware_button_config)
        await cg.register_parented(btn, var)
    if test_write_button_config := config.get(CONF_TEST_WRITE_BUTTON):
        btn = await button.new_button(test_write_button_config)
        await cg.register_parented(btn, var)
    if runtime_diagnostic_button_config := config.get(CONF_RUNTIME_DIAGNOSTIC_BUTTON):
        btn = await button.new_button(runtime_diagnostic_button_config)
        await cg.register_parented(btn, var)
