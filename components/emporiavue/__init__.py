import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor, button, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_RESET_PIN,
    CONF_STATUS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CHIP,
    ICON_DATABASE,
)

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["binary_sensor", "button", "text_sensor"]

emporiavue_ns = cg.esphome_ns.namespace("emporiavue")
EmporiaVueComponent = emporiavue_ns.class_(
    "EmporiaVueComponent", cg.Component
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

CONF_SWCLK_PIN = "swclk_pin"
CONF_SWDIO_PIN = "swdio_pin"
CONF_READ_BUTTON = "read_button"
CONF_PROBE_BUTTON = "probe_button"
CONF_DUMP_FLASH_BUTTON = "dump_flash_button"
CONF_SWD_IDCODE = "swd_idcode"
CONF_DSU_DID = "dsu_did"
CONF_READ_ALLOWED = "read_allowed"
CONF_DUMP_START_ADDRESS = "dump_start_address"
CONF_DUMP_BLOCK_SIZE = "dump_block_size"
CONF_DUMP_BLOCK_COUNT = "dump_block_count"
CONF_DUMP_HALT_CORE = "dump_halt_core"
CONF_DUMP_RESUME_BETWEEN_BLOCKS = "dump_resume_between_blocks"
CONF_RESET_BEFORE_READ = "reset_before_read"
CONF_CONNECT_UNDER_RESET = "connect_under_reset"
CONF_RESET_HOLD_TIME = "reset_hold_time"
CONF_RESET_RELEASE_TIME = "reset_release_time"
CONF_CLOCK_DELAY = "clock_delay"
CONF_RETRY_COUNT = "retry_count"
CONF_INIT_PINS_ON_BOOT = "init_pins_on_boot"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EmporiaVueComponent),
        cv.Optional(CONF_SWDIO_PIN, default="GPIO13"): pins.internal_gpio_input_pullup_pin_schema,
        cv.Optional(CONF_SWCLK_PIN, default="GPIO14"): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RESET_BEFORE_READ, default=False): cv.boolean,
        cv.Optional(CONF_CONNECT_UNDER_RESET, default=False): cv.boolean,
        cv.Optional(CONF_RESET_HOLD_TIME, default="100ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RESET_RELEASE_TIME, default="50ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CLOCK_DELAY, default=2): cv.int_range(min=0, max=50),
        cv.Optional(CONF_RETRY_COUNT, default=40): cv.int_range(min=1, max=255),
        cv.Optional(CONF_INIT_PINS_ON_BOOT, default=False): cv.boolean,
        cv.Optional(CONF_DUMP_START_ADDRESS, default=0): cv.int_range(min=0, max=0xFFFFFFFF),
        cv.Optional(CONF_DUMP_BLOCK_SIZE, default=64): cv.int_range(min=1, max=128),
        cv.Optional(CONF_DUMP_BLOCK_COUNT, default=5): cv.int_range(min=1, max=32),
        cv.Optional(CONF_DUMP_HALT_CORE, default=True): cv.boolean,
        cv.Optional(CONF_DUMP_RESUME_BETWEEN_BLOCKS, default=True): cv.boolean,
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
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    swdio_pin = await cg.gpio_pin_expression(config[CONF_SWDIO_PIN])
    cg.add(var.set_swdio_pin(swdio_pin))
    swclk_pin = await cg.gpio_pin_expression(config[CONF_SWCLK_PIN])
    cg.add(var.set_swclk_pin(swclk_pin))
    if reset_pin_config := config.get(CONF_RESET_PIN):
        reset_pin = await cg.gpio_pin_expression(reset_pin_config)
        cg.add(var.set_reset_pin(reset_pin))

    cg.add(var.set_reset_before_read(config[CONF_RESET_BEFORE_READ]))
    cg.add(var.set_connect_under_reset(config[CONF_CONNECT_UNDER_RESET]))
    cg.add(var.set_reset_hold_time(config[CONF_RESET_HOLD_TIME]))
    cg.add(var.set_reset_release_time(config[CONF_RESET_RELEASE_TIME]))
    cg.add(var.set_clock_delay_us(config[CONF_CLOCK_DELAY]))
    cg.add(var.set_retry_count(config[CONF_RETRY_COUNT]))
    cg.add(var.set_init_pins_on_boot(config[CONF_INIT_PINS_ON_BOOT]))
    cg.add(var.set_dump_start_address(config[CONF_DUMP_START_ADDRESS]))
    cg.add(var.set_dump_block_size(config[CONF_DUMP_BLOCK_SIZE]))
    cg.add(var.set_dump_block_count(config[CONF_DUMP_BLOCK_COUNT]))
    cg.add(var.set_dump_halt_core(config[CONF_DUMP_HALT_CORE]))
    cg.add(var.set_dump_resume_between_blocks(config[CONF_DUMP_RESUME_BETWEEN_BLOCKS]))

    if swd_idcode_config := config.get(CONF_SWD_IDCODE):
        sens = await text_sensor.new_text_sensor(swd_idcode_config)
        cg.add(var.set_swd_idcode_sensor(sens))
    if dsu_did_config := config.get(CONF_DSU_DID):
        sens = await text_sensor.new_text_sensor(dsu_did_config)
        cg.add(var.set_dsu_did_sensor(sens))
    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status_sensor(sens))
    if read_allowed_config := config.get(CONF_READ_ALLOWED):
        sens = await binary_sensor.new_binary_sensor(read_allowed_config)
        cg.add(var.set_read_allowed_sensor(sens))
    if read_button_config := config.get(CONF_READ_BUTTON):
        btn = await button.new_button(read_button_config)
        await cg.register_parented(btn, var)
    if probe_button_config := config.get(CONF_PROBE_BUTTON):
        btn = await button.new_button(probe_button_config)
        await cg.register_parented(btn, var)
    if dump_flash_button_config := config.get(CONF_DUMP_FLASH_BUTTON):
        btn = await button.new_button(dump_flash_button_config)
        await cg.register_parented(btn, var)
