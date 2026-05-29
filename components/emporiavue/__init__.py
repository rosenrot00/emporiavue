from pathlib import Path
import urllib.request

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import button, i2c, number, sensor, text_sensor
from esphome.const import (
    CONF_CALIBRATION,
    CONF_CURRENT,
    CONF_ID,
    CONF_INPUT,
    CONF_INITIAL_VALUE,
    CONF_NAME,
    CONF_PHASE_ANGLE,
    CONF_POWER,
    CONF_RESET_PIN,
    CONF_VOLTAGE,
    CONF_FREQUENCY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_DEGREES,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_WATT,
)

DEPENDENCIES = ["esp32", "i2c"]
AUTO_LOAD = ["button", "number", "sensor", "text_sensor"]

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
EmporiaVueFlashExternalFirmwareButton = emporiavue_ns.class_(
    "EmporiaVueFlashExternalFirmwareButton", button.Button
)
MeteringPhaseConfig = emporiavue_ns.class_("MeteringPhaseConfig")
MeteringCTClampConfig = emporiavue_ns.class_("MeteringCTClampConfig")
MeteringGroupConfig = emporiavue_ns.class_("MeteringGroupConfig")
MeteringCalibrationNumber = emporiavue_ns.class_(
    "MeteringCalibrationNumber", number.Number
)

CONF_SWCLK_PIN = "swclk_pin"
CONF_SWDIO_PIN = "swdio_pin"
CONF_BACKUP_FIRMWARE_BUTTON = "backup_firmware_button"
CONF_INSTALL_FIRMWARE_BUTTON = "install_firmware_button"
CONF_RESTORE_FIRMWARE_BUTTON = "restore_firmware_button"
CONF_EXTERNAL_SAMD_FIRMWARE = "external_samd_firmware"
CONF_BUTTON = "button"
CONF_EXTERNAL_FIRMWARE_ID = "id"
CONF_URL = "url"
CONF_TOKEN = "token"
CONF_DIAG_SAMPLE_BLOCKS = "diag_sample_blocks"
CONF_DIAG_PACKETS_BUILT = "diag_packets_built"
CONF_DIAG_PACKETS_READ = "diag_packets_read"
CONF_DIAG_DMA_TRANSFER_ERRORS = "diag_dma_transfer_errors"
CONF_DIAG_PACKET_OVERRUNS = "diag_packet_overruns"
CONF_DIAG_I2C_PARTIAL_READS = "diag_i2c_partial_reads"
CONF_DIAG_LAST_SAMPLE_COUNT = "diag_last_sample_count"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_BUNDLED_FIRMWARE_VERSION = "bundled_firmware_version"
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
CONF_DIAGNOSTICS_INTERVAL = "diagnostics_interval"
CONF_METERING_INTERVAL = "metering_interval"
CONF_GRID_DEADBAND = "grid_deadband"
CONF_TOTAL_POWER = "total_power"
CONF_GRID_IMPORT_POWER = "grid_import_power"
CONF_GRID_EXPORT_POWER = "grid_export_power"
CONF_MAINS = "mains"
CONF_CIRCUITS = "circuits"
CONF_GROUPS = "groups"
CONF_LINE = "line"
CONF_PHASE_ID = "phase_id"
CONF_CALIBRATION_NUMBER = "calibration_number"
CONF_MAIN_CLAMP = "main_clamp"
CONF_CT_ID = "ct_id"
CONF_VOLTAGE_INPUT = "voltage_input"
CONF_PHASES = "phases"
CONF_CT_CLAMPS = "ct_clamps"

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

PHASE_INPUTS = {
    "BLACK": 0,
    "RED": 1,
    "BLUE": 2,
}

CT_INPUTS = {
    "A": 0,
    "B": 1,
    "C": 2,
    "1": 3,
    "2": 4,
    "3": 5,
    "4": 6,
    "5": 7,
    "6": 8,
    "7": 9,
    "8": 10,
    "9": 11,
    "10": 12,
    "11": 13,
    "12": 14,
    "13": 15,
    "14": 16,
    "15": 17,
    "16": 18,
}

BRANCH_CT_INPUTS = {key: value for key, value in CT_INPUTS.items() if key not in {"A", "B", "C"}}

MAIN_PHASE_DEFAULTS = {
    "line_1": {
        "label": "Line 1",
    },
    "line_2": {
        "label": "Line 2",
    },
    "line_3": {
        "label": "Line 3",
    },
}

EXTERNAL_SAMD_FIRMWARE_HEADER = Path(__file__).with_name("external_samd_firmware.h")

CORE_ENTITY_NAMES = {
    CONF_FIRMWARE_VERSION: "SAMD Firmware Version",
    CONF_BUNDLED_FIRMWARE_VERSION: "SAMD Bundled Firmware Version",
    CONF_BACKUP_FIRMWARE_BUTTON: "Read SAMD Firmware",
    CONF_INSTALL_FIRMWARE_BUTTON: "Flash SAMD Bundled Firmware",
    CONF_RESTORE_FIRMWARE_BUTTON: "Flash SAMD Backup Firmware",
}

DIAGNOSTIC_ENTITY_NAMES = {
    CONF_DIAG_SAMPLE_BLOCKS: "SAMD Sample Blocks",
    CONF_DIAG_PACKETS_BUILT: "SAMD Packets Built",
    CONF_DIAG_PACKETS_READ: "SAMD Packets Read",
    CONF_DIAG_DMA_TRANSFER_ERRORS: "SAMD DMA Transfer Errors",
    CONF_DIAG_PACKET_OVERRUNS: "SAMD Packet Overruns",
    CONF_DIAG_I2C_PARTIAL_READS: "SAMD I2C Partial Reads",
    CONF_DIAG_LAST_SAMPLE_COUNT: "SAMD Last Sample Count",
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

    for key, default_name in CORE_ENTITY_NAMES.items():
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

    for key, default_name in DIAGNOSTIC_ENTITY_NAMES.items():
        entity_config = config.get(key)
        if entity_config is None or not isinstance(entity_config, dict):
            continue

        entity_config = dict(entity_config)
        if entity_config.get(CONF_NAME, default_name) == default_name:
            entity_config[CONF_NAME] = _prefixed_entity_name(prefix, default_name)

        config[key] = entity_config
    return config


def _apply_diagnostics_defaults(config):
    config = dict(config)
    if CONF_DIAGNOSTICS_INTERVAL not in config:
        return config

    for key, default_name in DIAGNOSTIC_ENTITY_NAMES.items():
        entity_config = config.get(key)
        if entity_config is None:
            entity_config = {}
        elif not isinstance(entity_config, dict):
            continue
        else:
            entity_config = dict(entity_config)

        entity_config.setdefault(CONF_NAME, default_name)
        config[key] = entity_config
    return config


def _apply_mains_defaults(config):
    config = dict(config)
    if CONF_MAINS not in config:
        return config

    prefix = config.get(CONF_ENTITY_PREFIX, "")
    mains = config[CONF_MAINS]
    if not isinstance(mains, dict):
        return config

    normalized_mains = dict(mains)
    for phase_key, defaults in MAIN_PHASE_DEFAULTS.items():
        if phase_key not in normalized_mains:
            continue
        phase_config = normalized_mains[phase_key]
        if phase_config is None:
            phase_config = {}
        elif not isinstance(phase_config, dict):
            raise cv.Invalid(f"mains.{phase_key} must be a mapping")
        else:
            phase_config = dict(phase_config)

        calibration_number_config = phase_config.get(CONF_CALIBRATION_NUMBER)
        default_name = f"{defaults['label']} Calibration"
        if calibration_number_config is None:
            calibration_number_config = {CONF_NAME: default_name}
            name_is_default = True
        elif not isinstance(calibration_number_config, dict):
            raise cv.Invalid(f"mains.{phase_key}.calibration_number must be a mapping")
        else:
            calibration_number_config = dict(calibration_number_config)
            name_is_default = calibration_number_config.get(CONF_NAME) in (None, default_name)

        if calibration_number_config.get(CONF_NAME) is None:
            calibration_number_config[CONF_NAME] = default_name
        if prefix and name_is_default:
            calibration_number_config[CONF_NAME] = _prefixed_entity_name(prefix, default_name)
        if CONF_INITIAL_VALUE not in calibration_number_config and CONF_CALIBRATION in phase_config:
            calibration_number_config[CONF_INITIAL_VALUE] = phase_config[CONF_CALIBRATION]
        calibration_number_config.setdefault(CONF_MODE, "BOX")
        phase_config[CONF_CALIBRATION_NUMBER] = calibration_number_config
        normalized_mains[phase_key] = phase_config

    config[CONF_MAINS] = normalized_mains
    return config


def _apply_external_firmware_defaults(config):
    config = dict(config)
    if CONF_EXTERNAL_SAMD_FIRMWARE not in config:
        return config

    prefix = config.get(CONF_ENTITY_PREFIX, "")
    raw_entries = config[CONF_EXTERNAL_SAMD_FIRMWARE]
    if isinstance(raw_entries, dict):
        entries = [dict(raw_entries)]
    elif isinstance(raw_entries, list):
        entries = [dict(entry) for entry in raw_entries]
    else:
        return config

    seen_ids = set()
    normalized_entries = []
    for index, entry in enumerate(entries):
        firmware_id = entry.get(CONF_EXTERNAL_FIRMWARE_ID)
        if firmware_id is None:
            if len(entries) == 1:
                firmware_id = "external"
            else:
                raise cv.Invalid("external_samd_firmware entries need an id when multiple firmwares are configured")
        firmware_id = str(firmware_id)
        if firmware_id in seen_ids:
            raise cv.Invalid(f"duplicate external_samd_firmware id: {firmware_id}")
        seen_ids.add(firmware_id)
        entry[CONF_EXTERNAL_FIRMWARE_ID] = firmware_id

        button_config = entry.get(CONF_BUTTON)
        if firmware_id == "external":
            firmware_name = "External"
        else:
            firmware_name = firmware_id.replace("_", " ").replace("-", " ").title()
        default_name = f"Flash SAMD {firmware_name} Firmware"

        if button_config is None:
            button_config = {CONF_NAME: default_name}
            name_is_default = True
        elif not isinstance(button_config, dict):
            raise cv.Invalid("external_samd_firmware button must be a mapping")
        else:
            button_config = dict(button_config)
            name_is_default = button_config.get(CONF_NAME) in (None, default_name)

        if button_config.get(CONF_NAME) is None:
            button_config[CONF_NAME] = default_name
        if prefix and name_is_default:
            button_config[CONF_NAME] = _prefixed_entity_name(prefix, default_name)

        entry[CONF_BUTTON] = button_config
        normalized_entries.append(entry)

    config[CONF_EXTERNAL_SAMD_FIRMWARE] = normalized_entries
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
    return _apply_entity_name_defaults(
        _apply_diagnostics_defaults(
            _apply_mains_defaults(_apply_external_firmware_defaults(_apply_hardware_defaults(config)))
        )
    )


def _download_external_samd_firmware(url, token=None):
    headers = {"User-Agent": "ESPHome emporiavue"}
    if token:
        token = token.strip()
        headers["Authorization"] = token if " " in token else f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
    except Exception as err:
        raise cv.Invalid(f"external_samd_firmware download failed: {err}") from err

    if not data:
        raise cv.Invalid("external_samd_firmware URL returned an empty file")
    return data


def _external_samd_firmware_header(firmwares=None):
    if not firmwares:
        return """#pragma once

#include <cstdint>

namespace esphome {
namespace emporiavue {

static constexpr uint8_t EXTERNAL_SAMD_FIRMWARE_COUNT = 0;
static constexpr uint8_t EXTERNAL_SAMD_FIRMWARE_0[1] = {0x00};
static constexpr uint32_t EXTERNAL_SAMD_FIRMWARE_SIZES[1] = {0UL};
static constexpr const uint8_t *EXTERNAL_SAMD_FIRMWARE_IMAGES[1] = {EXTERNAL_SAMD_FIRMWARE_0};

}  // namespace emporiavue
}  // namespace esphome
"""

    arrays = []
    sizes = []
    images = []
    for index, firmware in enumerate(firmwares):
        data = firmware["data"]
        rows = []
        for offset in range(0, len(data), 12):
            chunk = data[offset : offset + 12]
            rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
        body = "\n".join(rows)
        arrays.append(
            f"""static constexpr uint8_t EXTERNAL_SAMD_FIRMWARE_{index}[{len(data)}] = {{
{body}
}};"""
        )
        sizes.append(f"{len(data)}UL")
        images.append(f"EXTERNAL_SAMD_FIRMWARE_{index}")
    array_body = "\n\n".join(arrays)
    size_body = ", ".join(sizes)
    image_body = ", ".join(images)
    return f"""#pragma once

#include <cstdint>

namespace esphome {{
namespace emporiavue {{

static constexpr uint8_t EXTERNAL_SAMD_FIRMWARE_COUNT = {len(firmwares)};
{array_body}

static constexpr uint32_t EXTERNAL_SAMD_FIRMWARE_SIZES[EXTERNAL_SAMD_FIRMWARE_COUNT] = {{{size_body}}};
static constexpr const uint8_t *EXTERNAL_SAMD_FIRMWARE_IMAGES[EXTERNAL_SAMD_FIRMWARE_COUNT] = {{{image_body}}};

}}  // namespace emporiavue
}}  // namespace esphome
"""


def _write_external_samd_firmware_header(firmwares=None):
    try:
        EXTERNAL_SAMD_FIRMWARE_HEADER.write_text(
            _external_samd_firmware_header(firmwares), encoding="utf-8"
        )
    except OSError as err:
        raise cv.Invalid(f"external_samd_firmware header generation failed: {err}") from err


CALIBRATION_NUMBER_SCHEMA = number.number_schema(
    MeteringCalibrationNumber,
    icon="mdi:tune",
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.Optional(CONF_INITIAL_VALUE): cv.positive_float,
    }
)


PHASE_VOLTAGE_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

PHASE_FREQUENCY_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_HERTZ,
    device_class=DEVICE_CLASS_FREQUENCY,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

PHASE_ANGLE_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=0,
)

POWER_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

CURRENT_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)


def _validate_metering_phases(value):
    phases = cv.Schema(
        cv.ensure_list(
            {
                cv.Required(CONF_ID): cv.declare_id(MeteringPhaseConfig),
                cv.Required(CONF_INPUT): cv.one_of(*PHASE_INPUTS.keys(), upper=True),
                cv.Optional(CONF_CALIBRATION, default=0.022): cv.positive_float,
                cv.Optional(CONF_CALIBRATION_NUMBER): CALIBRATION_NUMBER_SCHEMA,
                cv.Optional(CONF_VOLTAGE): PHASE_VOLTAGE_SENSOR_SCHEMA,
                cv.Optional(CONF_FREQUENCY): PHASE_FREQUENCY_SENSOR_SCHEMA,
                cv.Optional(CONF_PHASE_ANGLE): PHASE_ANGLE_SENSOR_SCHEMA,
            }
        )
    )(value)

    if len(phases) > 3:
        raise cv.Invalid("No more than 3 metering phases are supported")

    inputs = [phase[CONF_INPUT] for phase in phases]
    if len(inputs) != len(set(inputs)):
        raise cv.Invalid("Only one metering phase entry per input color is allowed")

    for index, phase in enumerate(phases):
        input_wire = phase[CONF_INPUT]
        if CONF_CALIBRATION_NUMBER in phase:
            calibration_number_config = dict(phase[CONF_CALIBRATION_NUMBER])
            calibration_number_config.setdefault(CONF_INITIAL_VALUE, phase[CONF_CALIBRATION])
            phase[CONF_CALIBRATION_NUMBER] = calibration_number_config
        if input_wire == "BLACK" and CONF_PHASE_ANGLE in phase:
            raise cv.Invalid(
                "Phase angle is not supported for the black wire, only for red and blue",
                path=[index, CONF_PHASE_ANGLE],
            )
        if input_wire in {"RED", "BLUE"} and CONF_FREQUENCY in phase:
            raise cv.Invalid(
                "Frequency is not supported for red and blue, only for the black wire",
                path=[index, CONF_FREQUENCY],
            )

    return phases


METERING_MAIN_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MeteringPhaseConfig),
        cv.GenerateID(CONF_CT_ID): cv.declare_id(MeteringCTClampConfig),
        cv.Required(CONF_VOLTAGE_INPUT): cv.one_of(*PHASE_INPUTS.keys(), upper=True),
        cv.Required(CONF_MAIN_CLAMP): cv.one_of("A", "B", "C", upper=True),
        cv.Required(CONF_CALIBRATION): cv.positive_float,
        cv.Optional(CONF_CALIBRATION_NUMBER): CALIBRATION_NUMBER_SCHEMA,
        cv.Optional(CONF_VOLTAGE): PHASE_VOLTAGE_SENSOR_SCHEMA,
        cv.Optional(CONF_FREQUENCY): PHASE_FREQUENCY_SENSOR_SCHEMA,
        cv.Optional(CONF_PHASE_ANGLE): PHASE_ANGLE_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT): CURRENT_SENSOR_SCHEMA,
    }
)


def _validate_mains(value):
    mains = cv.Schema(
        {
            cv.Optional("line_1"): METERING_MAIN_SCHEMA,
            cv.Optional("line_2"): METERING_MAIN_SCHEMA,
            cv.Optional("line_3"): METERING_MAIN_SCHEMA,
        }
    )(value)

    inputs = [phase_config[CONF_VOLTAGE_INPUT] for phase_config in mains.values()]
    if len(inputs) != len(set(inputs)):
        raise cv.Invalid("Only one main phase entry per voltage input color is allowed")

    clamps = [phase_config[CONF_MAIN_CLAMP] for phase_config in mains.values()]
    if len(clamps) != len(set(clamps)):
        raise cv.Invalid("Only one main phase entry per main CT clamp is allowed")

    for phase_key, phase_config in mains.items():
        input_wire = phase_config[CONF_VOLTAGE_INPUT]
        if input_wire == "BLACK" and CONF_PHASE_ANGLE in phase_config:
            raise cv.Invalid(
                "Phase angle is not supported for the black wire, only for red and blue",
                path=[phase_key, CONF_PHASE_ANGLE],
            )
        if input_wire in {"RED", "BLUE"} and CONF_FREQUENCY in phase_config:
            raise cv.Invalid(
                "Frequency is not supported for red or blue, only for the black wire",
                path=[phase_key, CONF_FREQUENCY],
            )

    return mains


METERING_CIRCUIT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CT_ID): cv.declare_id(MeteringCTClampConfig),
        cv.Required(CONF_INPUT): cv.one_of(*BRANCH_CT_INPUTS.keys(), upper=True),
        cv.Required(CONF_LINE): cv.int_range(min=1, max=3),
        cv.Optional(CONF_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT): CURRENT_SENSOR_SCHEMA,
    }
)


def _validate_circuits(value):
    circuits = cv.Schema({cv.string_strict: METERING_CIRCUIT_SCHEMA})(value)

    inputs = [circuit_config[CONF_INPUT] for circuit_config in circuits.values()]
    if len(inputs) != len(set(inputs)):
        raise cv.Invalid("Only one circuit entry per branch CT input is allowed")

    return circuits


METERING_GROUP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MeteringGroupConfig),
        cv.Required(CONF_CIRCUITS): cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_POWER): POWER_SENSOR_SCHEMA,
    }
)


def _validate_groups(value):
    groups = cv.Schema({cv.string_strict: METERING_GROUP_SCHEMA})(value)

    for group_key, group_config in groups.items():
        circuits = group_config[CONF_CIRCUITS]
        if not circuits:
            raise cv.Invalid("Group needs at least one circuit", path=[group_key, CONF_CIRCUITS])
        if len(circuits) != len(set(circuits)):
            raise cv.Invalid("Group circuit entries must be unique", path=[group_key, CONF_CIRCUITS])

    return groups


def _validate_metering_topology(config):
    circuits = config.get(CONF_CIRCUITS, {})
    mains = config.get(CONF_MAINS, {})

    for circuit_key, circuit_config in circuits.items():
        line_key = f"line_{circuit_config[CONF_LINE]}"
        if line_key not in mains:
            raise cv.Invalid(
                f"circuits.{circuit_key}.line references {line_key}, but mains.{line_key} is not configured"
            )

    for group_key, group_config in config.get(CONF_GROUPS, {}).items():
        for circuit_key in group_config[CONF_CIRCUITS]:
            if circuit_key not in circuits:
                raise cv.Invalid(
                    f"groups.{group_key}.circuits references {circuit_key}, but circuits.{circuit_key} is not configured"
                )

    return config


METERING_CT_CLAMP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MeteringCTClampConfig),
        cv.Required(CONF_PHASE_ID): cv.use_id(MeteringPhaseConfig),
        cv.Required(CONF_INPUT): cv.one_of(*CT_INPUTS.keys(), upper=True),
        cv.Optional(CONF_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT): CURRENT_SENSOR_SCHEMA,
    }
)


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
        cv.Optional(CONF_DIAGNOSTICS_INTERVAL): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_METERING_INTERVAL): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_GRID_DEADBAND, default=2.0): cv.positive_float,
        cv.Optional(CONF_TOTAL_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_GRID_IMPORT_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_GRID_EXPORT_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_MAINS): _validate_mains,
        cv.Optional(CONF_CIRCUITS): _validate_circuits,
        cv.Optional(CONF_GROUPS): _validate_groups,
        cv.Optional(CONF_PHASES): _validate_metering_phases,
        cv.Optional(CONF_CT_CLAMPS): cv.ensure_list(METERING_CT_CLAMP_SCHEMA),
        cv.Optional(CONF_BACKUP_PARTITION, default="samd_bak"): cv.string_strict,
        cv.Optional(CONF_EXTERNAL_SAMD_FIRMWARE): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_EXTERNAL_FIRMWARE_ID): cv.string_strict,
                    cv.Required(CONF_URL): cv.string_strict,
                    cv.Optional(CONF_TOKEN): cv.string_strict,
                    cv.Required(CONF_BUTTON): button.button_schema(
                        EmporiaVueFlashExternalFirmwareButton,
                        icon="mdi:web",
                        entity_category=ENTITY_CATEGORY_CONFIG,
                    ),
                }
            )
        ),
        cv.Optional(
            CONF_FIRMWARE_VERSION,
            default={CONF_NAME: "SAMD Firmware Version"},
        ): text_sensor.text_sensor_schema(
            icon="mdi:chip",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_BUNDLED_FIRMWARE_VERSION,
            default={CONF_NAME: "SAMD Bundled Firmware Version"},
        ): text_sensor.text_sensor_schema(
            icon="mdi:package-variant",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_SAMPLE_BLOCKS,
        ): sensor.sensor_schema(
            icon="mdi:counter",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_PACKETS_BUILT,
        ): sensor.sensor_schema(
            icon="mdi:counter",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_PACKETS_READ,
        ): sensor.sensor_schema(
            icon="mdi:counter",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_DMA_TRANSFER_ERRORS,
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_PACKET_OVERRUNS,
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_I2C_PARTIAL_READS,
        ): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_DIAG_LAST_SAMPLE_COUNT,
        ): sensor.sensor_schema(
            icon="mdi:counter",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(
            CONF_BACKUP_FIRMWARE_BUTTON,
            default={CONF_NAME: "Read SAMD Firmware"},
        ): button.button_schema(
            EmporiaVueBackupFirmwareButton,
            icon="mdi:content-save",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_INSTALL_FIRMWARE_BUTTON,
            default={CONF_NAME: "Flash SAMD Bundled Firmware"},
        ): button.button_schema(
            EmporiaVueInstallFirmwareButton,
            icon="mdi:package-variant-closed",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(
            CONF_RESTORE_FIRMWARE_BUTTON,
            default={CONF_NAME: "Flash SAMD Backup Firmware"},
        ): button.button_schema(
            EmporiaVueRestoreFirmwareButton,
            icon="mdi:backup-restore",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x64))

CONFIG_SCHEMA = cv.All(_apply_defaults, EMPORIAVUE_SCHEMA, _validate_metering_topology)


async def to_code(config):
    external_firmwares = []
    for external_firmware_config in config.get(CONF_EXTERNAL_SAMD_FIRMWARE, []):
        external_firmwares.append(
            {
                CONF_EXTERNAL_FIRMWARE_ID: external_firmware_config[CONF_EXTERNAL_FIRMWARE_ID],
                "data": _download_external_samd_firmware(
                    external_firmware_config[CONF_URL],
                    external_firmware_config.get(CONF_TOKEN),
                ),
            }
        )
    _write_external_samd_firmware_header(external_firmwares)

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
    if diagnostics_interval := config.get(CONF_DIAGNOSTICS_INTERVAL):
        cg.add(var.set_diagnostics_interval(diagnostics_interval))
    if metering_interval := config.get(CONF_METERING_INTERVAL):
        cg.add(var.set_metering_interval(metering_interval))
    cg.add(var.set_grid_deadband(config[CONF_GRID_DEADBAND]))
    cg.add(var.set_backup_partition_name(config[CONF_BACKUP_PARTITION]))
    if total_power_config := config.get(CONF_TOTAL_POWER):
        sens = await sensor.new_sensor(total_power_config)
        cg.add(var.set_total_power_sensor(sens))
    if grid_import_power_config := config.get(CONF_GRID_IMPORT_POWER):
        sens = await sensor.new_sensor(grid_import_power_config)
        cg.add(var.set_grid_import_power_sensor(sens))
    if grid_export_power_config := config.get(CONF_GRID_EXPORT_POWER):
        sens = await sensor.new_sensor(grid_export_power_config)
        cg.add(var.set_grid_export_power_sensor(sens))
    if firmware_version_config := config.get(CONF_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(firmware_version_config)
        cg.add(var.set_firmware_version_sensor(sens))
    if bundled_firmware_version_config := config.get(CONF_BUNDLED_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(bundled_firmware_version_config)
        cg.add(var.set_bundled_firmware_version_sensor(sens))
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
    if diag_last_sample_count_config := config.get(CONF_DIAG_LAST_SAMPLE_COUNT):
        sens = await sensor.new_sensor(diag_last_sample_count_config)
        cg.add(var.set_diag_last_sample_count_sensor(sens))

    phases = []
    ct_clamps = []
    main_phase_vars_by_line = {}
    circuit_ct_clamps_by_key = {}
    for phase_key, main_config in config.get(CONF_MAINS, {}).items():
        phase_var = cg.new_Pvariable(main_config[CONF_ID], MeteringPhaseConfig())
        cg.add(phase_var.set_input_wire(PHASE_INPUTS[main_config[CONF_VOLTAGE_INPUT]]))
        cg.add(phase_var.set_calibration(main_config[CONF_CALIBRATION]))
        line_number = int(phase_key.rsplit("_", 1)[1])
        main_phase_vars_by_line[line_number] = phase_var

        if calibration_number_config := main_config.get(CONF_CALIBRATION_NUMBER):
            cal_num = await number.new_number(
                calibration_number_config,
                min_value=0.001,
                max_value=0.1,
                step=0.000001,
            )
            cg.add(cal_num.set_initial_value(calibration_number_config[CONF_INITIAL_VALUE]))
            await cg.register_parented(cal_num, phase_var)
            cg.add(phase_var.set_calibration_number(cal_num))

        if voltage_config := main_config.get(CONF_VOLTAGE):
            sens = await sensor.new_sensor(voltage_config)
            cg.add(phase_var.set_voltage_sensor(sens))
        if frequency_config := main_config.get(CONF_FREQUENCY):
            sens = await sensor.new_sensor(frequency_config)
            cg.add(phase_var.set_frequency_sensor(sens))
        if phase_angle_config := main_config.get(CONF_PHASE_ANGLE):
            sens = await sensor.new_sensor(phase_angle_config)
            cg.add(phase_var.set_phase_angle_sensor(sens))

        phases.append(phase_var)

        ct_clamp_var = cg.new_Pvariable(main_config[CONF_CT_ID], MeteringCTClampConfig())
        cg.add(ct_clamp_var.set_phase(phase_var))
        cg.add(ct_clamp_var.set_input_port(CT_INPUTS[main_config[CONF_MAIN_CLAMP]]))
        if power_config := main_config.get(CONF_POWER):
            sens = await sensor.new_sensor(power_config)
            cg.add(ct_clamp_var.set_power_sensor(sens))
        if current_config := main_config.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(ct_clamp_var.set_current_sensor(sens))
        ct_clamps.append(ct_clamp_var)

    for circuit_key, circuit_config in config.get(CONF_CIRCUITS, {}).items():
        ct_clamp_var = cg.new_Pvariable(circuit_config[CONF_CT_ID], MeteringCTClampConfig())
        phase_var = main_phase_vars_by_line[circuit_config[CONF_LINE]]
        cg.add(ct_clamp_var.set_phase(phase_var))
        cg.add(ct_clamp_var.set_input_port(BRANCH_CT_INPUTS[circuit_config[CONF_INPUT]]))

        if power_config := circuit_config.get(CONF_POWER):
            sens = await sensor.new_sensor(power_config)
            cg.add(ct_clamp_var.set_power_sensor(sens))
        if current_config := circuit_config.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(ct_clamp_var.set_current_sensor(sens))

        ct_clamps.append(ct_clamp_var)
        circuit_ct_clamps_by_key[circuit_key] = ct_clamp_var

    for phase_config in config.get(CONF_PHASES, []):
        phase_var = cg.new_Pvariable(phase_config[CONF_ID], MeteringPhaseConfig())
        cg.add(phase_var.set_input_wire(PHASE_INPUTS[phase_config[CONF_INPUT]]))
        cg.add(phase_var.set_calibration(phase_config[CONF_CALIBRATION]))

        if calibration_number_config := phase_config.get(CONF_CALIBRATION_NUMBER):
            cal_num = await number.new_number(
                calibration_number_config,
                min_value=0.001,
                max_value=0.1,
                step=0.000001,
            )
            cg.add(cal_num.set_initial_value(calibration_number_config[CONF_INITIAL_VALUE]))
            await cg.register_parented(cal_num, phase_var)
            cg.add(phase_var.set_calibration_number(cal_num))

        if voltage_config := phase_config.get(CONF_VOLTAGE):
            sens = await sensor.new_sensor(voltage_config)
            cg.add(phase_var.set_voltage_sensor(sens))
        if frequency_config := phase_config.get(CONF_FREQUENCY):
            sens = await sensor.new_sensor(frequency_config)
            cg.add(phase_var.set_frequency_sensor(sens))
        if phase_angle_config := phase_config.get(CONF_PHASE_ANGLE):
            sens = await sensor.new_sensor(phase_angle_config)
            cg.add(phase_var.set_phase_angle_sensor(sens))

        phases.append(phase_var)
    if phases:
        cg.add(var.set_metering_phases(phases))

    for ct_config in config.get(CONF_CT_CLAMPS, []):
        ct_clamp_var = cg.new_Pvariable(ct_config[CONF_ID], MeteringCTClampConfig())
        phase_var = await cg.get_variable(ct_config[CONF_PHASE_ID])
        cg.add(ct_clamp_var.set_phase(phase_var))
        cg.add(ct_clamp_var.set_input_port(CT_INPUTS[ct_config[CONF_INPUT]]))

        if power_config := ct_config.get(CONF_POWER):
            sens = await sensor.new_sensor(power_config)
            cg.add(ct_clamp_var.set_power_sensor(sens))
        if current_config := ct_config.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(ct_clamp_var.set_current_sensor(sens))

        ct_clamps.append(ct_clamp_var)
    if ct_clamps:
        cg.add(var.set_metering_ct_clamps(ct_clamps))

    groups = []
    for group_config in config.get(CONF_GROUPS, {}).values():
        group_var = cg.new_Pvariable(group_config[CONF_ID], MeteringGroupConfig())
        group_ct_clamps = [
            circuit_ct_clamps_by_key[circuit_key] for circuit_key in group_config[CONF_CIRCUITS]
        ]
        cg.add(group_var.set_ct_clamps(group_ct_clamps))
        if power_config := group_config.get(CONF_POWER):
            sens = await sensor.new_sensor(power_config)
            cg.add(group_var.set_power_sensor(sens))
        groups.append(group_var)
    if groups:
        cg.add(var.set_metering_groups(groups))

    if backup_firmware_button_config := config.get(CONF_BACKUP_FIRMWARE_BUTTON):
        btn = await button.new_button(backup_firmware_button_config)
        await cg.register_parented(btn, var)
    if install_firmware_button_config := config.get(CONF_INSTALL_FIRMWARE_BUTTON):
        btn = await button.new_button(install_firmware_button_config)
        await cg.register_parented(btn, var)
    if restore_firmware_button_config := config.get(CONF_RESTORE_FIRMWARE_BUTTON):
        btn = await button.new_button(restore_firmware_button_config)
        await cg.register_parented(btn, var)
    for index, external_firmware_config in enumerate(config.get(CONF_EXTERNAL_SAMD_FIRMWARE, [])):
        btn = await button.new_button(external_firmware_config[CONF_BUTTON])
        cg.add(btn.set_firmware_index(index))
        await cg.register_parented(btn, var)
