from pathlib import Path
import copy
import hashlib
import struct
import urllib.request

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import pins
from esphome.components import (
    button,
    i2c,
    number,
    select,
    sensor,
    text_sensor,
    time,
)
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_CURRENT,
    CONF_DEVICE_ID,
    CONF_DEVICES,
    CONF_ESPHOME,
    CONF_ID,
    CONF_INPUT,
    CONF_INITIAL_VALUE,
    CONF_METHOD,
    CONF_NAME,
    CONF_PHASE_ANGLE,
    CONF_POWER,
    CONF_RESET_PIN,
    CONF_RESTORE,
    CONF_TIME_ID,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VOLTAGE,
    CONF_FREQUENCY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DATA_SIZE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RESTART,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_BYTES,
    UNIT_DEGREES,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.core import ID
from esphome.core.config import DEVICE_SCHEMA, Device

DEPENDENCIES = ["esp32"]
AUTO_LOAD = [
    "button",
    "number",
    "select",
    "sensor",
    "text_sensor",
    "time",
    "total_daily_energy",
]

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
MeteringVirtualLineConfig = emporiavue_ns.class_("MeteringVirtualLineConfig")
MeteringCalibrationNumber = emporiavue_ns.class_(
    "MeteringCalibrationNumber", number.Number
)
MeteringCurrentGainNumber = emporiavue_ns.class_(
    "MeteringCurrentGainNumber", number.Number
)
MeteringCurrentPhaseNumber = emporiavue_ns.class_(
    "MeteringCurrentPhaseNumber", number.Number
)
MeteringLineSelect = emporiavue_ns.class_("MeteringLineSelect", select.Select)
total_daily_energy_ns = cg.esphome_ns.namespace("total_daily_energy")
TotalDailyEnergyMethod = total_daily_energy_ns.enum("TotalDailyEnergyMethod")
TOTAL_DAILY_ENERGY_METHODS = {
    "trapezoid": TotalDailyEnergyMethod.TOTAL_DAILY_ENERGY_METHOD_TRAPEZOID,
    "left": TotalDailyEnergyMethod.TOTAL_DAILY_ENERGY_METHOD_LEFT,
    "right": TotalDailyEnergyMethod.TOTAL_DAILY_ENERGY_METHOD_RIGHT,
}
TotalDailyEnergy = total_daily_energy_ns.class_(
    "TotalDailyEnergy", sensor.Sensor, cg.Component
)

CONF_SWCLK_PIN = "swclk_pin"
CONF_SWDIO_PIN = "swdio_pin"
CONF_SPI_CLK_PIN = "spi_clk_pin"
CONF_SPI_DATA_PIN = "spi_data_pin"
CONF_SPI_FRAME_PIN = "spi_frame_pin"
CONF_SPI_MAIN_CURRENT_DELAY = "spi_main_current_delay"
CONF_SPI_MUX_CURRENT_DELAY = "spi_mux_current_delay"
CONF_BACKUP_FIRMWARE_BUTTON = "backup_firmware_button"
CONF_INSTALL_FIRMWARE_BUTTON = "install_firmware_button"
CONF_RESTORE_FIRMWARE_BUTTON = "restore_firmware_button"
CONF_EXTERNAL_SAMD_FIRMWARE = "external_samd_firmware"
CONF_BUTTON = "button"
CONF_EXTERNAL_FIRMWARE_ID = "id"
CONF_URL = "url"
CONF_TOKEN = "token"
CONF_DIAG_FRAME_ERRORS = "diag_frame_errors"
CONF_DIAG_TRANSFER_ERRORS = "diag_transfer_errors"
CONF_DIAG_FRAME_OVERRUNS = "diag_frame_overruns"
CONF_DIAG_RECOVERIES = "diag_recoveries"
CONF_DIAG_LAST_FRAME_SAMPLES = "diag_last_frame_samples"
CONF_DIAG_SAMPLE_RATE = "diag_sample_rate"
CONF_DIAG_HEAP_FREE = "diag_heap_free"
CONF_DIAG_HEAP_MINIMUM = "diag_heap_minimum"
CONF_DIAG_LOOP_STACK_FREE = "diag_loop_stack_free"
CONF_DIAG_SPI_STACK_FREE = "diag_spi_stack_free"
CONF_DIAG_SPI_PROCESSING_LOAD = "diag_spi_processing_load"
CONF_DIAG_SPI_PROCESSING_OVERRUNS = "diag_spi_processing_overruns"
CONF_DIAG_RESTART_REASON = "diag_restart_reason"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_BUNDLED_FIRMWARE_VERSION = "bundled_firmware_version"
CONF_BACKUP_PARTITION = "backup_partition"
CONF_HARDWARE = "hardware"
CONF_CONNECT_UNDER_RESET = "connect_under_reset"
CONF_SWD_ON_BOOT = "swd_on_boot"
CONF_RESET_RELEASE_TIME = "reset_release_time"
CONF_CLOCK_DELAY = "clock_delay"
CONF_MODE = "mode"
CONF_ENTITY_PREFIX = "entity_prefix"
CONF_FORCE_ENTITY_PREFIX = "force_entity_prefix"
CONF_AUTO_UPDATE_SAMD = "auto_update_samd"
CONF_DIAGNOSTICS_INTERVAL = "diagnostics_interval"
CONF_METERING_INTERVAL = "metering_interval"
CONF_MINIMUM_APPARENT_POWER = "minimum_apparent_power"
CONF_MINIMUM_FUNDAMENTAL_CURRENT = "minimum_fundamental_current"
CONF_DEMAND_INTERVAL = "demand_interval"
CONF_PEAK_INTERVAL = "peak_interval"
CONF_PHASE_DETECTION = "phase_detection"
CONF_LINE_SELECT = "line_select"
CONF_POWER_MIN = "power_min"
CONF_CONFIDENCE_RATIO = "confidence_ratio"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_ENTITY_CATEGORY = "entity_category"
CONF_RAW_POWER = "raw_power"
CONF_MAINS = "mains"
CONF_CIRCUITS = "circuits"
CONF_ESPHOME_SUBDEVICES = "esphome_subdevices"
CONF_GROUPS = "groups"
CONF_SOURCES = "sources"
CONF_SOURCES_TO_SUBDEVICE = "sources_to_subdevice"
CONF_VIRTUAL_LINES = "virtual_lines"
CONF_LINES = "lines"
CONF_LINE = "line"
CONF_DIRECTION = "direction"
CONF_POWER_APPARENT = "power_apparent"
CONF_POWER_FACTOR = "power_factor"
CONF_FUNDAMENTAL_CURRENT = "fundamental_current"
CONF_FUNDAMENTAL_REACTIVE_POWER = "fundamental_reactive_power"
CONF_FUNDAMENTAL_POWER_FACTOR = "fundamental_power_factor"
CONF_DISPLACEMENT_ANGLE = "displacement_angle"
CONF_CURRENT_THD = "current_thd"
CONF_VOLTAGE_THD = "voltage_thd"
CONF_POWER_DEMAND = "power_demand"
CONF_MAXIMUM_POWER_DEMAND = "maximum_power_demand"
CONF_CURRENT_DEMAND = "current_demand"
CONF_MAXIMUM_CURRENT_DEMAND = "maximum_current_demand"
CONF_CURRENT_PEAK = "current_peak"
CONF_CURRENT_CREST_FACTOR = "current_crest_factor"
CONF_POWER_SPLIT = "power_split"
CONF_PHASE_ID = "phase_id"
CONF_VOLTAGE_CALIBRATION = "voltage_calibration"
CONF_VOLTAGE_CALIBRATION_NUMBER = "voltage_calibration_number"
CONF_CURRENT_CALIBRATION = "current_calibration"
CONF_GAIN = "gain"
CONF_PHASE = "phase"
CONF_GAIN_NUMBER = "gain_number"
CONF_PHASE_NUMBER = "phase_number"
CONF_MAIN_CLAMP = "main_clamp"
CONF_CT_ID = "ct_id"
CONF_VOLTAGE_INPUT = "voltage_input"
CONF_PHASES = "phases"
CONF_CT_CLAMPS = "ct_clamps"
CONF_INTERNAL = "internal"
CONF_FILTERS = "filters"
CONF_FILTER_DEFAULTS = "filter_defaults"
CONF_MULTIPLY = "multiply"
CONF_LAMBDA = "lambda"
CONF_ENERGY = "energy"
CONF_STATE_CLASS = "state_class"
STATE_CLASS_TOTAL = "total"

SPI_ANALYSIS_SENSOR_KEYS = (
    CONF_VOLTAGE_THD,
    CONF_FUNDAMENTAL_CURRENT,
    CONF_FUNDAMENTAL_REACTIVE_POWER,
    CONF_FUNDAMENTAL_POWER_FACTOR,
    CONF_DISPLACEMENT_ANGLE,
    CONF_CURRENT_THD,
    CONF_CURRENT_PEAK,
    CONF_CURRENT_CREST_FACTOR,
)

CIRCUIT_DIRECT_ENTITY_KEYS = (
    CONF_LINE_SELECT,
    CONF_CURRENT,
    CONF_POWER_APPARENT,
    CONF_POWER_FACTOR,
    CONF_FUNDAMENTAL_CURRENT,
    CONF_FUNDAMENTAL_REACTIVE_POWER,
    CONF_FUNDAMENTAL_POWER_FACTOR,
    CONF_DISPLACEMENT_ANGLE,
    CONF_CURRENT_THD,
    CONF_POWER_DEMAND,
    CONF_MAXIMUM_POWER_DEMAND,
    CONF_CURRENT_DEMAND,
    CONF_MAXIMUM_CURRENT_DEMAND,
    CONF_CURRENT_PEAK,
    CONF_CURRENT_CREST_FACTOR,
    CONF_PHASE_DETECTION,
)

MAIN_DIRECT_ENTITY_KEYS = (
    CONF_VOLTAGE,
    CONF_FREQUENCY,
    CONF_PHASE_ANGLE,
    CONF_VOLTAGE_THD,
    CONF_VOLTAGE_CALIBRATION_NUMBER,
    CONF_CURRENT,
    CONF_POWER_APPARENT,
    CONF_POWER_FACTOR,
    CONF_FUNDAMENTAL_CURRENT,
    CONF_FUNDAMENTAL_REACTIVE_POWER,
    CONF_FUNDAMENTAL_POWER_FACTOR,
    CONF_DISPLACEMENT_ANGLE,
    CONF_CURRENT_THD,
    CONF_POWER_DEMAND,
    CONF_MAXIMUM_POWER_DEMAND,
    CONF_CURRENT_DEMAND,
    CONF_MAXIMUM_CURRENT_DEMAND,
    CONF_CURRENT_PEAK,
    CONF_CURRENT_CREST_FACTOR,
)

HARDWARE_CUSTOM = "custom"
HARDWARE_VUE2 = "vue2"
HARDWARE_VUE3 = "vue3"
MODE_I2C = "i2c"
MODE_SPI = "spi"
DIRECTION_BOTH = "both"
DIRECTION_SIGNED_ALIAS = "signed"
DIRECTION_POSITIVE = "positive"
DIRECTION_NEGATIVE = "negative"

HARDWARE_IDS = {
    HARDWARE_CUSTOM: 0,
    HARDWARE_VUE2: 2,
    HARDWARE_VUE3: 3,
}

MODE_IDS = {
    MODE_I2C: 0,
    MODE_SPI: 1,
}

POWER_DIRECTION_IDS = {
    DIRECTION_BOTH: 0,
    DIRECTION_POSITIVE: 1,
    DIRECTION_NEGATIVE: 2,
}

POWER_DIRECTION_ALIASES = {
    DIRECTION_BOTH: DIRECTION_BOTH,
    DIRECTION_SIGNED_ALIAS: DIRECTION_BOTH,
    DIRECTION_POSITIVE: DIRECTION_POSITIVE,
    DIRECTION_NEGATIVE: DIRECTION_NEGATIVE,
}

FIRMWARE_MODE_IDS = {
    MODE_I2C: 1,
    MODE_SPI: 2,
}

HARDWARE_SLUGS = {
    HARDWARE_VUE2: "vue2",
    HARDWARE_VUE3: "vue3",
}

MODE_SLUGS = {
    MODE_I2C: "i2c",
    MODE_SPI: "spi",
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

MAIN_CT_INPUTS_BY_HARDWARE = {
    HARDWARE_VUE2: {
        "A": 0,
        "B": 1,
        "C": 2,
    },
    HARDWARE_VUE3: {
        "A": 2,
        "B": 1,
        "C": 0,
    },
}


def _main_ct_input_for_hardware(hardware, main_clamp):
    return MAIN_CT_INPUTS_BY_HARDWARE.get(hardware, CT_INPUTS)[main_clamp]

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
BUNDLED_SAMD_FIRMWARE_HEADER = Path(__file__).with_name("samd09_firmware.h")
BUNDLED_SAMD_FIRMWARE_FOOTER_FORMAT = "<HHI32s15s9s"
BUNDLED_SAMD_FIRMWARE_MARKER = b"EMPORIAVUE-SAMD"

CORE_ENTITY_NAMES = {
    CONF_FIRMWARE_VERSION: "SAMD Firmware Version",
    CONF_BUNDLED_FIRMWARE_VERSION: "SAMD Bundled Firmware Version",
    CONF_BACKUP_FIRMWARE_BUTTON: "Read SAMD Firmware",
    CONF_INSTALL_FIRMWARE_BUTTON: "Flash SAMD Bundled Firmware",
    CONF_RESTORE_FIRMWARE_BUTTON: "Flash SAMD Backup Firmware",
}

DIAGNOSTIC_ENTITY_NAMES = {
    CONF_DIAG_FRAME_ERRORS: "SAMD Frame Errors",
    CONF_DIAG_TRANSFER_ERRORS: "SAMD Transfer Errors",
    CONF_DIAG_FRAME_OVERRUNS: "SAMD Frame Overruns",
    CONF_DIAG_RECOVERIES: "SAMD Recoveries",
    CONF_DIAG_LAST_FRAME_SAMPLES: "SAMD Last Frame Samples",
    CONF_DIAG_SAMPLE_RATE: "SAMD Sample Rate",
    CONF_DIAG_RESTART_REASON: "ESP Restart Reason",
}

SPI_RUNTIME_DIAGNOSTIC_ENTITY_NAMES = {
    CONF_DIAG_TRANSFER_ERRORS: "ESP SPI Transfer Errors",
    CONF_DIAG_HEAP_FREE: "ESP Free Heap",
    CONF_DIAG_HEAP_MINIMUM: "ESP Minimum Free Heap",
    CONF_DIAG_LOOP_STACK_FREE: "ESP Loop Minimum Free Stack",
    CONF_DIAG_SPI_STACK_FREE: "ESP SPI Minimum Free Stack",
    CONF_DIAG_SPI_PROCESSING_LOAD: "ESP SPI Processing Load",
    CONF_DIAG_SPI_PROCESSING_OVERRUNS: "ESP SPI Processing Overruns",
}


def _diagnostic_entity_names(config):
    names = dict(DIAGNOSTIC_ENTITY_NAMES)
    if config.get(CONF_MODE, MODE_I2C) == MODE_SPI:
        names.update(SPI_RUNTIME_DIAGNOSTIC_ENTITY_NAMES)
    return names


class _DefaultEntityName(str):
    """Marks names generated by this component rather than supplied by the user."""


def _default_entity_name(name):
    return _DefaultEntityName(name)


def _prefixed_entity_name(prefix, name):
    if not prefix:
        return name
    full_prefix = f"{prefix} "
    if name == prefix or name.startswith(full_prefix):
        return name
    prefixed_name = f"{full_prefix}{name}"
    if isinstance(name, _DefaultEntityName):
        return _default_entity_name(prefixed_name)
    return prefixed_name


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

    for key, default_name in _diagnostic_entity_names(config).items():
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

    for key, default_name in _diagnostic_entity_names(config).items():
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

        if CONF_VOLTAGE_CALIBRATION_NUMBER not in phase_config:
            normalized_mains[phase_key] = phase_config
            continue

        calibration_number_config = phase_config[CONF_VOLTAGE_CALIBRATION_NUMBER]
        default_name = _default_entity_name(f"{defaults['label']} Voltage Calibration")
        if calibration_number_config is None or calibration_number_config is True:
            calibration_number_config = {CONF_NAME: default_name}
            name_is_default = True
        elif not isinstance(calibration_number_config, dict):
            raise cv.Invalid(f"mains.{phase_key}.voltage_calibration_number must be a mapping")
        else:
            calibration_number_config = dict(calibration_number_config)
            name_is_default = calibration_number_config.get(CONF_NAME) in (None, default_name)

        if calibration_number_config.get(CONF_NAME) is None:
            calibration_number_config[CONF_NAME] = default_name
        if prefix and name_is_default:
            calibration_number_config[CONF_NAME] = _prefixed_entity_name(prefix, default_name)
        if CONF_INITIAL_VALUE not in calibration_number_config and CONF_VOLTAGE_CALIBRATION in phase_config:
            calibration_number_config[CONF_INITIAL_VALUE] = phase_config[CONF_VOLTAGE_CALIBRATION]
        calibration_number_config.setdefault(CONF_MODE, "BOX")
        phase_config[CONF_VOLTAGE_CALIBRATION_NUMBER] = calibration_number_config
        normalized_mains[phase_key] = phase_config

    config[CONF_MAINS] = normalized_mains
    return config


def _base_name_from_power_name(name):
    if name.endswith(" Power"):
        return name[:-6]
    if name.endswith(" power"):
        return name[:-6]
    return name


def _first_power_config(parent_config):
    power_config = parent_config.get(CONF_POWER)
    if isinstance(power_config, list):
        return power_config[0] if power_config else None
    if isinstance(power_config, dict):
        return power_config
    return None


def _main_default_base_name(main_key, main_config):
    return main_config.get(CONF_NAME, main_key.replace("_", " ").title())


def _circuit_default_base_name(circuit_key, circuit_config):
    if CONF_NAME in circuit_config:
        return circuit_config[CONF_NAME]
    power_config = _first_power_config(circuit_config)
    if isinstance(power_config, dict) and power_config.get(CONF_NAME):
        return _base_name_from_power_name(power_config[CONF_NAME])
    if circuit_key.startswith("cir"):
        return f"Circuit {circuit_key.removeprefix('cir')}"
    return circuit_key.replace("_", " ").title()


def _circuit_default_power_name(circuit_key, circuit_config):
    power_config = _first_power_config(circuit_config)
    if isinstance(power_config, dict) and power_config.get(CONF_NAME):
        return power_config[CONF_NAME]
    return f"{_circuit_default_base_name(circuit_key, circuit_config)} Power"


def _circuit_default_phase_name(circuit_key, circuit_config):
    return f"{_circuit_default_base_name(circuit_key, circuit_config)} Phase"


def _group_default_base_name(group_key, group_config):
    if CONF_NAME in group_config:
        return group_config[CONF_NAME]
    power_config = _first_power_config(group_config)
    if isinstance(power_config, dict) and power_config.get(CONF_NAME):
        return _base_name_from_power_name(power_config[CONF_NAME])
    return _base_name_from_power_name(group_key.replace("_", " ").title())


def _group_default_power_name(group_key, group_config):
    power_config = _first_power_config(group_config)
    if isinstance(power_config, dict) and power_config.get(CONF_NAME):
        return power_config[CONF_NAME]
    return f"{_group_default_base_name(group_key, group_config)} Power"


def _energy_default_name(base_name):
    return _default_entity_name(f"{base_name} Today's Energy")


def _direction_energy_state_class(direction):
    if direction == DIRECTION_BOTH:
        return STATE_CLASS_TOTAL
    return STATE_CLASS_TOTAL_INCREASING


def _apply_energy_sensor_defaults(parent_config, default_name, default_state_class=None):
    if CONF_ENERGY not in parent_config:
        return parent_config

    energy_config = parent_config[CONF_ENERGY]
    if energy_config is False:
        parent_config = dict(parent_config)
        parent_config.pop(CONF_ENERGY, None)
        return parent_config
    if energy_config is True or energy_config is None:
        energy_config = {}
    elif not isinstance(energy_config, dict):
        raise cv.Invalid("energy must be true or a mapping")
    else:
        energy_config = dict(energy_config)

    if CONF_NAME not in energy_config and CONF_ID not in energy_config:
        energy_config[CONF_NAME] = _default_entity_name(default_name)
    energy_config.setdefault(CONF_UNIT_OF_MEASUREMENT, "kWh")
    energy_config.setdefault(CONF_ACCURACY_DECIMALS, 2)
    if default_state_class is not None:
        energy_config.setdefault(CONF_STATE_CLASS, default_state_class)
    energy_config.setdefault(CONF_RESTORE, True)
    energy_config.setdefault(CONF_METHOD, "left")

    parent_config = dict(parent_config)
    parent_config[CONF_ENERGY] = energy_config
    return parent_config


def _normalize_visible_sensor_config(sensor_config, path):
    if sensor_config is True:
        return {}
    if sensor_config is None:
        return {}
    if not isinstance(sensor_config, dict):
        raise cv.Invalid(f"{path} must be true or a mapping")
    return dict(sensor_config)


def _virtual_line_default_voltage_name(virtual_line_key, virtual_line_config):
    lines = virtual_line_config.get(CONF_LINES)
    if isinstance(lines, list) and len(lines) == 2:
        try:
            return f"Line {int(lines[0])}-{int(lines[1])} Voltage"
        except (TypeError, ValueError):
            pass
    return f"{virtual_line_key.replace('_', ' ').title()} Voltage"


def _direction_power_suffix(direction):
    return {
        DIRECTION_BOTH: "Power",
        DIRECTION_POSITIVE: "Import Power",
        DIRECTION_NEGATIVE: "Export Power",
    }[direction]


def _direction_energy_suffix(direction):
    return {
        DIRECTION_BOTH: "Energy",
        DIRECTION_POSITIVE: "Import Energy",
        DIRECTION_NEGATIVE: "Export Energy",
    }[direction]


def _direction_id_suffix(direction):
    return {
        DIRECTION_BOTH: "",
        DIRECTION_POSITIVE: "_positive",
        DIRECTION_NEGATIVE: "_negative",
    }[direction]


def _direction_power_name(base_name, direction):
    return _default_entity_name(f"{base_name} {_direction_power_suffix(direction)}")


def _direction_energy_name(base_name, direction):
    return _default_entity_name(f"{base_name} Today's {_direction_energy_suffix(direction)}")


def _normalize_power_direction(direction):
    return POWER_DIRECTION_ALIASES.get(str(direction).lower())


def _normalize_power_output_list(power_config, path, create_when_empty=False):
    if power_config is None:
        return [{}] if create_when_empty else []
    if power_config is True:
        return [{}]
    if power_config is False:
        return []
    if isinstance(power_config, dict):
        direction_keys = set(POWER_DIRECTION_ALIASES)
        if CONF_DIRECTION not in power_config and direction_keys.intersection(power_config):
            extra_keys = set(power_config) - direction_keys
            if extra_keys:
                allowed = ", ".join((DIRECTION_BOTH, DIRECTION_POSITIVE, DIRECTION_NEGATIVE))
                raise cv.Invalid(f"{path}.power direction map only supports {allowed}")
            outputs = []
            seen_directions = set()
            for key, output_config in power_config.items():
                direction = _normalize_power_direction(key)
                if direction in seen_directions:
                    raise cv.Invalid(f"{path}.power has more than one {direction} output")
                seen_directions.add(direction)
                if output_config is None or output_config is True:
                    output_config = {}
                elif output_config is False:
                    continue
                elif not isinstance(output_config, dict):
                    raise cv.Invalid(f"{path}.power.{key} must be a mapping")
                else:
                    output_config = dict(output_config)
                configured_direction = _normalize_power_direction(output_config.get(CONF_DIRECTION, direction))
                if configured_direction != direction:
                    raise cv.Invalid(f"{path}.power.{key}.direction must be {direction}")
                output_config[CONF_DIRECTION] = direction
                outputs.append(output_config)
            return outputs
        return [dict(power_config)]
    if isinstance(power_config, list):
        outputs = []
        for index, output_config in enumerate(power_config):
            if output_config is None or output_config is True:
                outputs.append({})
            elif output_config is False:
                continue
            elif isinstance(output_config, dict):
                outputs.append(dict(output_config))
            else:
                raise cv.Invalid(f"{path}.power[{index}] must be a mapping")
        return outputs
    raise cv.Invalid(f"{path}.power must be a mapping or a list of mappings")


def _apply_power_output_defaults(parent_config, raw_id, base_name, path, always_create_both=False):
    parent_config = dict(parent_config)
    power_outputs = _normalize_power_output_list(
        parent_config.get(CONF_POWER), path, create_when_empty=CONF_POWER in parent_config
    )

    has_parent_energy_config = CONF_ENERGY in parent_config
    parent_energy_config = parent_config.pop(CONF_ENERGY, None)
    parent_energy_output = None
    if has_parent_energy_config and parent_energy_config is not False:
        if not power_outputs:
            power_outputs.append({})
        both_output = None
        for output_config in power_outputs:
            if _normalize_power_direction(output_config.get(CONF_DIRECTION, DIRECTION_BOTH)) == DIRECTION_BOTH:
                both_output = output_config
                break
        if both_output is None:
            both_output = {CONF_DIRECTION: DIRECTION_BOTH}
            power_outputs.insert(0, both_output)
        both_output[CONF_ENERGY] = parent_energy_config
        parent_energy_output = both_output

    if not power_outputs and always_create_both:
        power_outputs.append({CONF_DIRECTION: DIRECTION_BOTH})

    normalized_outputs = []
    seen_directions = set()
    for index, output_config in enumerate(power_outputs):
        direction = _normalize_power_direction(output_config.get(CONF_DIRECTION, DIRECTION_BOTH))
        if direction is None:
            raise cv.Invalid(
                f"{path}.power[{index}].direction must be both, positive, or negative"
            )
        if direction in seen_directions:
            raise cv.Invalid(f"{path}.power has more than one {direction} output")
        seen_directions.add(direction)

        output_config = dict(output_config)
        output_config[CONF_DIRECTION] = direction

        raw_config = output_config.get(CONF_RAW_POWER)
        if raw_config is None:
            raw_config = {}
        elif not isinstance(raw_config, dict):
            raise cv.Invalid(f"{path}.power[{index}].raw_power must be a mapping")
        else:
            raw_config = dict(raw_config)

        raw_config.setdefault(CONF_ID, output_config.pop(CONF_ID, f"{raw_id}{_direction_id_suffix(direction)}"))
        raw_config.setdefault(CONF_INTERNAL, True)
        output_config[CONF_RAW_POWER] = raw_config

        if CONF_NAME not in output_config:
            output_config[CONF_NAME] = _direction_power_name(base_name, direction)
        if CONF_ENERGY in output_config:
            energy_state_class = (
                STATE_CLASS_TOTAL_INCREASING
                if output_config is parent_energy_output
                else _direction_energy_state_class(direction)
            )
            output_config = _apply_energy_sensor_defaults(
                output_config,
                _direction_energy_name(base_name, direction),
                energy_state_class,
            )
        normalized_outputs.append(output_config)

    if normalized_outputs:
        parent_config[CONF_POWER] = normalized_outputs
    else:
        parent_config.pop(CONF_POWER, None)
    return parent_config


def _name_from_power_name(power_name, replacement):
    if power_name.endswith(" Power"):
        return f"{power_name[:-6]} {replacement}"
    if power_name.endswith(" power"):
        return f"{power_name[:-6]} {replacement.lower()}"
    return f"{power_name} {replacement}"


def _base_name_from_power_name(power_name):
    if power_name.endswith(" Power") or power_name.endswith(" power"):
        return power_name[:-6]
    return power_name


def _power_split_line_key(line_number):
    return f"line_{line_number}"


def _apply_power_split_defaults(circuit_key, circuit_config, default_name):
    if CONF_POWER_SPLIT not in circuit_config:
        return circuit_config

    line_config = circuit_config.get(CONF_LINE)
    if not isinstance(line_config, list) or len(line_config) != 2:
        raise cv.Invalid(f"circuits.{circuit_key}.power_split needs line: [line_a, line_b]")

    power_split_config = circuit_config[CONF_POWER_SPLIT]
    if power_split_config is False or power_split_config is None:
        circuit_config = dict(circuit_config)
        circuit_config.pop(CONF_POWER_SPLIT, None)
        return circuit_config
    if power_split_config is True:
        power_split_config = {}
    elif not isinstance(power_split_config, dict):
        raise cv.Invalid(f"circuits.{circuit_key}.power_split must be true or a mapping")
    else:
        power_split_config = dict(power_split_config)

    allowed_keys = [_power_split_line_key(line_number) for line_number in line_config]
    unknown_keys = [key for key in power_split_config if key not in allowed_keys]
    if unknown_keys:
        allowed = ", ".join(allowed_keys)
        raise cv.Invalid(
            f"circuits.{circuit_key}.power_split only supports {allowed} for line: {line_config}"
        )

    if not power_split_config:
        requested_keys = allowed_keys
    else:
        requested_keys = list(power_split_config.keys())

    normalized = {}
    for line_key in requested_keys:
        line_number = int(line_key.rsplit("_", 1)[1])
        sensor_config = power_split_config.get(line_key)
        if sensor_config is None:
            sensor_config = {}
        elif sensor_config is True:
            sensor_config = {}
        elif not isinstance(sensor_config, dict):
            raise cv.Invalid(f"circuits.{circuit_key}.power_split.{line_key} must be true or a mapping")
        else:
            sensor_config = dict(sensor_config)

        if CONF_NAME not in sensor_config and CONF_ID not in sensor_config:
            sensor_config[CONF_NAME] = _default_entity_name(
                _name_from_power_name(default_name, f"Line {line_number} Share")
            )
        normalized[line_key] = sensor_config

    circuit_config = dict(circuit_config)
    circuit_config[CONF_POWER_SPLIT] = normalized
    return circuit_config


def _apply_optional_sensor_default_name(parent_config, key, default_name):
    if key not in parent_config:
        return parent_config

    sensor_config = parent_config[key]
    if sensor_config is None or sensor_config is True:
        sensor_config = {}
    elif not isinstance(sensor_config, dict):
        raise cv.Invalid(f"{key} must be true or a mapping")
    else:
        sensor_config = dict(sensor_config)

    if CONF_NAME not in sensor_config and CONF_ID not in sensor_config:
        sensor_config[CONF_NAME] = _default_entity_name(default_name)

    parent_config = dict(parent_config)
    parent_config[key] = sensor_config
    return parent_config


def _apply_demand_default_names(parent_config, base_name, include_current=True):
    names = (
        (CONF_POWER_DEMAND, f"{base_name} Power Demand"),
        (CONF_MAXIMUM_POWER_DEMAND, f"Today's {base_name} Maximum Power Demand"),
    )
    if include_current:
        names += (
            (CONF_CURRENT_DEMAND, f"{base_name} Current Demand"),
            (CONF_MAXIMUM_CURRENT_DEMAND, f"Today's {base_name} Maximum Current Demand"),
        )
    for key, default_name in names:
        parent_config = _apply_optional_sensor_default_name(parent_config, key, default_name)
    return parent_config


def _apply_peak_default_names(parent_config, base_name):
    for key, default_name in (
        (CONF_CURRENT_PEAK, f"{base_name} Current Peak"),
        (CONF_CURRENT_CREST_FACTOR, f"{base_name} Current Crest Factor"),
    ):
        parent_config = _apply_optional_sensor_default_name(parent_config, key, default_name)
    return parent_config


def _apply_current_calibration_defaults(parent_config, base_name):
    if CONF_CURRENT_CALIBRATION not in parent_config:
        return parent_config
    calibration = parent_config[CONF_CURRENT_CALIBRATION]
    if not isinstance(calibration, dict):
        return parent_config

    calibration = dict(calibration)
    for number_key, value_key, default_name, default_value in (
        (CONF_GAIN_NUMBER, CONF_GAIN, f"{base_name} Current Gain Calibration", 1.0),
        (CONF_PHASE_NUMBER, CONF_PHASE, f"{base_name} Current Phase Calibration", 0.0),
    ):
        if number_key not in calibration:
            continue
        number_config = calibration[number_key]
        if number_config is None or number_config is True:
            number_config = {}
        elif not isinstance(number_config, dict):
            continue
        else:
            number_config = dict(number_config)
        if CONF_NAME not in number_config:
            number_config[CONF_NAME] = _default_entity_name(default_name)
        number_config.setdefault(CONF_INITIAL_VALUE, calibration.get(value_key, default_value))
        number_config.setdefault(CONF_MODE, "BOX")
        calibration[number_key] = number_config

    parent_config = dict(parent_config)
    parent_config[CONF_CURRENT_CALIBRATION] = calibration
    return parent_config


def _copy_filter_default(filter_defaults, key):
    if not isinstance(filter_defaults, dict) or key not in filter_defaults:
        return None
    return copy.deepcopy(filter_defaults[key])


def _apply_filter_default_to_sensor(parent_config, key, filter_defaults, path):
    if key not in parent_config:
        return parent_config

    default_filters = _copy_filter_default(filter_defaults, key)
    if default_filters is None:
        return parent_config

    sensor_config = parent_config[key]
    if sensor_config is False:
        return parent_config
    if sensor_config is True or sensor_config is None:
        sensor_config = {}
    elif not isinstance(sensor_config, dict):
        raise cv.Invalid(f"{path}.{key} must be true or a mapping")
    else:
        sensor_config = dict(sensor_config)

    if CONF_FILTERS not in sensor_config:
        sensor_config[CONF_FILTERS] = default_filters

    parent_config = dict(parent_config)
    parent_config[key] = sensor_config
    return parent_config


def _apply_filter_defaults_to_power_outputs(parent_config, filter_defaults, path):
    if CONF_POWER not in parent_config:
        return parent_config

    parent_config = dict(parent_config)
    power_outputs = []
    for index, output_config in enumerate(parent_config[CONF_POWER]):
        if not isinstance(output_config, dict):
            power_outputs.append(output_config)
            continue

        output_config = dict(output_config)
        default_filters = _copy_filter_default(filter_defaults, CONF_POWER)
        if default_filters is not None and CONF_FILTERS not in output_config:
            output_config[CONF_FILTERS] = default_filters
        output_config = _apply_filter_default_to_sensor(
            output_config, CONF_ENERGY, filter_defaults, f"{path}.power[{index}]"
        )
        power_outputs.append(output_config)

    parent_config[CONF_POWER] = power_outputs
    return parent_config


def _apply_filter_defaults_to_power_split(parent_config, filter_defaults, path):
    if CONF_POWER_SPLIT not in parent_config or not isinstance(parent_config[CONF_POWER_SPLIT], dict):
        return parent_config

    parent_config = dict(parent_config)
    power_split = {}
    for line_key, sensor_config in parent_config[CONF_POWER_SPLIT].items():
        line_parent = {CONF_POWER: sensor_config}
        line_parent = _apply_filter_default_to_sensor(
            line_parent, CONF_POWER, filter_defaults, f"{path}.power_split.{line_key}"
        )
        power_split[line_key] = line_parent[CONF_POWER]

    parent_config[CONF_POWER_SPLIT] = power_split
    return parent_config


def _apply_filter_defaults_to_metering_node(node_config, filter_defaults, path, phase_sensors=False):
    if not isinstance(node_config, dict):
        return node_config

    node_config = dict(node_config)
    if phase_sensors:
        for key in (CONF_VOLTAGE, CONF_FREQUENCY, CONF_PHASE_ANGLE, CONF_VOLTAGE_THD):
            node_config = _apply_filter_default_to_sensor(node_config, key, filter_defaults, path)

    for key in (
        CONF_CURRENT,
        CONF_POWER_APPARENT,
        CONF_POWER_FACTOR,
        CONF_FUNDAMENTAL_CURRENT,
        CONF_FUNDAMENTAL_REACTIVE_POWER,
        CONF_FUNDAMENTAL_POWER_FACTOR,
        CONF_DISPLACEMENT_ANGLE,
        CONF_CURRENT_THD,
        CONF_POWER_DEMAND,
        CONF_MAXIMUM_POWER_DEMAND,
        CONF_CURRENT_DEMAND,
        CONF_MAXIMUM_CURRENT_DEMAND,
        CONF_CURRENT_PEAK,
        CONF_CURRENT_CREST_FACTOR,
    ):
        node_config = _apply_filter_default_to_sensor(node_config, key, filter_defaults, path)
    node_config = _apply_filter_defaults_to_power_outputs(node_config, filter_defaults, path)
    node_config = _apply_filter_defaults_to_power_split(node_config, filter_defaults, path)
    return node_config


def _apply_filter_defaults(config):
    filter_defaults = config.get(CONF_FILTER_DEFAULTS)
    if not isinstance(filter_defaults, dict):
        return config

    config = dict(config)

    if CONF_MAINS in config and isinstance(config[CONF_MAINS], dict):
        mains = dict(config[CONF_MAINS])
        for main_key, main_config in list(mains.items()):
            mains[main_key] = _apply_filter_defaults_to_metering_node(
                main_config, filter_defaults, f"mains.{main_key}", phase_sensors=True
            )
        config[CONF_MAINS] = mains

    if CONF_CIRCUITS in config and isinstance(config[CONF_CIRCUITS], dict):
        circuits = dict(config[CONF_CIRCUITS])
        for circuit_key, circuit_config in list(circuits.items()):
            circuits[circuit_key] = _apply_filter_defaults_to_metering_node(
                circuit_config, filter_defaults, f"circuits.{circuit_key}"
            )
        config[CONF_CIRCUITS] = circuits

    if CONF_GROUPS in config and isinstance(config[CONF_GROUPS], dict):
        groups = dict(config[CONF_GROUPS])
        for group_key, group_config in list(groups.items()):
            groups[group_key] = _apply_filter_defaults_to_metering_node(
                group_config, filter_defaults, f"groups.{group_key}"
            )
        config[CONF_GROUPS] = groups

    if CONF_VIRTUAL_LINES in config and isinstance(config[CONF_VIRTUAL_LINES], dict):
        virtual_lines = dict(config[CONF_VIRTUAL_LINES])
        for virtual_line_key, virtual_line_config in list(virtual_lines.items()):
            virtual_lines[virtual_line_key] = _apply_filter_default_to_sensor(
                virtual_line_config,
                CONF_VOLTAGE,
                filter_defaults,
                f"virtual_lines.{virtual_line_key}",
            )
        config[CONF_VIRTUAL_LINES] = virtual_lines

    if CONF_PHASES in config and isinstance(config[CONF_PHASES], list):
        phases = []
        for index, phase_config in enumerate(config[CONF_PHASES]):
            phases.append(
                _apply_filter_defaults_to_metering_node(
                    phase_config, filter_defaults, f"phases[{index}]", phase_sensors=True
                )
            )
        config[CONF_PHASES] = phases

    if CONF_CT_CLAMPS in config and isinstance(config[CONF_CT_CLAMPS], list):
        ct_clamps = []
        for index, ct_config in enumerate(config[CONF_CT_CLAMPS]):
            ct_clamps.append(
                _apply_filter_defaults_to_metering_node(
                    ct_config, filter_defaults, f"ct_clamps[{index}]"
                )
            )
        config[CONF_CT_CLAMPS] = ct_clamps

    return config


def _parse_group_source(source):
    if not isinstance(source, str):
        raise cv.Invalid("Group source must be a string")

    sign = 1.0
    source = source.strip()
    if not source:
        raise cv.Invalid("Group source must not be empty")
    if source[0] in "+-":
        if source[0] == "-":
            sign = -1.0
        source = source[1:].strip()
    if not source:
        raise cv.Invalid("Group source sign needs a source name")

    return sign, source


def _normalize_group_sources(sources, path):
    normalized_sources = []
    seen_sources = {}
    for source in sources:
        sign, source_key = _parse_group_source(source)
        if source_key in seen_sources:
            if seen_sources[source_key] == sign:
                continue
            raise cv.Invalid("Group sources must not contain the same source with opposite signs", path=path)
        seen_sources[source_key] = sign
        normalized_sources.append(f"-{source_key}" if sign < 0 else source_key)
    return normalized_sources


def _apply_raw_power_defaults(config):
    config = dict(config)

    if CONF_MAINS in config and isinstance(config[CONF_MAINS], dict):
        mains = dict(config[CONF_MAINS])
        for main_key, main_config in list(mains.items()):
            if isinstance(main_config, dict):
                default_base_name = _main_default_base_name(main_key, main_config)
                main_config = _apply_power_output_defaults(
                    main_config,
                    f"{main_key}_power",
                    default_base_name,
                    f"mains.{main_key}",
                )
                default_name = _direction_power_name(default_base_name, DIRECTION_BOTH)
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_CURRENT, _name_from_power_name(default_name, "Current")
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_POWER_APPARENT, _name_from_power_name(default_name, "Apparent Power")
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_POWER_FACTOR, _name_from_power_name(default_name, "Power Factor")
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_FUNDAMENTAL_CURRENT, f"{default_base_name} Fundamental Current"
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config,
                    CONF_FUNDAMENTAL_REACTIVE_POWER,
                    f"{default_base_name} Fundamental Reactive Power",
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config,
                    CONF_FUNDAMENTAL_POWER_FACTOR,
                    f"{default_base_name} Fundamental Power Factor",
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_DISPLACEMENT_ANGLE, f"{default_base_name} Displacement Angle"
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_CURRENT_THD, f"{default_base_name} Current THD"
                )
                main_config = _apply_optional_sensor_default_name(
                    main_config, CONF_VOLTAGE_THD, f"{default_base_name} Voltage THD"
                )
                main_config = _apply_demand_default_names(main_config, default_base_name)
                main_config = _apply_peak_default_names(main_config, default_base_name)
                main_config = _apply_current_calibration_defaults(main_config, default_base_name)
                mains[main_key] = main_config
        config[CONF_MAINS] = mains

    if CONF_CIRCUITS in config and isinstance(config[CONF_CIRCUITS], dict):
        circuits = dict(config[CONF_CIRCUITS])
        for circuit_key, circuit_config in list(circuits.items()):
            if isinstance(circuit_config, dict):
                default_base_name = _circuit_default_base_name(circuit_key, circuit_config)
                circuits[circuit_key] = _apply_power_output_defaults(
                    circuit_config,
                    circuit_key,
                    default_base_name,
                    f"circuits.{circuit_key}",
                )
                default_name = _direction_power_name(default_base_name, DIRECTION_BOTH)
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key], CONF_CURRENT, _name_from_power_name(default_name, "Current")
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key],
                    CONF_POWER_APPARENT,
                    _name_from_power_name(default_name, "Apparent Power"),
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key],
                    CONF_POWER_FACTOR,
                    _name_from_power_name(default_name, "Power Factor"),
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key], CONF_FUNDAMENTAL_CURRENT, f"{default_base_name} Fundamental Current"
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key],
                    CONF_FUNDAMENTAL_REACTIVE_POWER,
                    f"{default_base_name} Fundamental Reactive Power",
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key],
                    CONF_FUNDAMENTAL_POWER_FACTOR,
                    f"{default_base_name} Fundamental Power Factor",
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key], CONF_DISPLACEMENT_ANGLE, f"{default_base_name} Displacement Angle"
                )
                circuits[circuit_key] = _apply_optional_sensor_default_name(
                    circuits[circuit_key], CONF_CURRENT_THD, f"{default_base_name} Current THD"
                )
                circuits[circuit_key] = _apply_demand_default_names(
                    circuits[circuit_key], default_base_name
                )
                circuits[circuit_key] = _apply_peak_default_names(
                    circuits[circuit_key], default_base_name
                )
                circuits[circuit_key] = _apply_current_calibration_defaults(
                    circuits[circuit_key], default_base_name
                )
                circuits[circuit_key] = _apply_power_split_defaults(
                    circuit_key, circuits[circuit_key], default_name
                )
        config[CONF_CIRCUITS] = circuits

    if CONF_GROUPS in config and isinstance(config[CONF_GROUPS], dict):
        groups = dict(config[CONF_GROUPS])
        for group_key, group_config in list(groups.items()):
            if isinstance(group_config, dict):
                default_base_name = _group_default_base_name(group_key, group_config)
                groups[group_key] = _apply_power_output_defaults(
                    group_config,
                    group_key,
                    default_base_name,
                    f"groups.{group_key}",
                )
                groups[group_key] = _apply_demand_default_names(
                    groups[group_key], default_base_name, include_current=False
                )
        config[CONF_GROUPS] = groups

    if CONF_CT_CLAMPS in config and isinstance(config[CONF_CT_CLAMPS], list):
        ct_clamps = []
        for index, ct_config in enumerate(config[CONF_CT_CLAMPS]):
            if isinstance(ct_config, dict):
                default_name = ct_config.get(CONF_NAME, f"CT {ct_config.get(CONF_INPUT, index + 1)} Power")
                default_base_name = _base_name_from_power_name(default_name)
                ct_config = _apply_power_output_defaults(
                    ct_config,
                    f"ct_{index}_power",
                    default_base_name,
                    f"ct_clamps[{index}]",
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config, CONF_CURRENT, _name_from_power_name(default_name, "Current")
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config, CONF_POWER_APPARENT, _name_from_power_name(default_name, "Apparent Power")
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config, CONF_POWER_FACTOR, _name_from_power_name(default_name, "Power Factor")
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config, CONF_FUNDAMENTAL_CURRENT, f"{default_base_name} Fundamental Current"
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config,
                    CONF_FUNDAMENTAL_REACTIVE_POWER,
                    f"{default_base_name} Fundamental Reactive Power",
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config,
                    CONF_FUNDAMENTAL_POWER_FACTOR,
                    f"{default_base_name} Fundamental Power Factor",
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config, CONF_DISPLACEMENT_ANGLE, f"{default_base_name} Displacement Angle"
                )
                ct_config = _apply_optional_sensor_default_name(
                    ct_config, CONF_CURRENT_THD, f"{default_base_name} Current THD"
                )
                ct_config = _apply_demand_default_names(ct_config, default_base_name)
                ct_config = _apply_peak_default_names(ct_config, default_base_name)
                ct_config = _apply_current_calibration_defaults(ct_config, default_base_name)
                ct_clamps.append(ct_config)
            else:
                ct_clamps.append(ct_config)
        config[CONF_CT_CLAMPS] = ct_clamps

    return config


def _apply_virtual_line_defaults(config):
    config = dict(config)
    if CONF_VIRTUAL_LINES not in config or not isinstance(config[CONF_VIRTUAL_LINES], dict):
        return config

    prefix = config.get(CONF_ENTITY_PREFIX, "")
    virtual_lines = dict(config[CONF_VIRTUAL_LINES])
    for virtual_line_key, virtual_line_config in list(virtual_lines.items()):
        if virtual_line_config is None:
            virtual_line_config = {}
        elif not isinstance(virtual_line_config, dict):
            continue
        else:
            virtual_line_config = dict(virtual_line_config)

        voltage_config = virtual_line_config.get(CONF_VOLTAGE)
        voltage_config = _normalize_visible_sensor_config(
            voltage_config, f"virtual_lines.{virtual_line_key}.voltage"
        )

        default_name = _virtual_line_default_voltage_name(virtual_line_key, virtual_line_config)
        name_is_default = voltage_config.get(CONF_NAME) in (None, default_name)
        if voltage_config.get(CONF_NAME) is None:
            voltage_config[CONF_NAME] = default_name
        if prefix and name_is_default:
            voltage_config[CONF_NAME] = _prefixed_entity_name(prefix, default_name)

        virtual_line_config[CONF_VOLTAGE] = voltage_config
        virtual_lines[virtual_line_key] = virtual_line_config

    config[CONF_VIRTUAL_LINES] = virtual_lines
    return config


def _apply_phase_detection_defaults(config):
    config = dict(config)
    if CONF_CIRCUITS not in config or not isinstance(config[CONF_CIRCUITS], dict):
        return config

    prefix = config.get(CONF_ENTITY_PREFIX, "")
    circuits = dict(config[CONF_CIRCUITS])
    for circuit_key, circuit_config in list(circuits.items()):
        if not isinstance(circuit_config, dict):
            continue

        circuit_config = dict(circuit_config)
        if CONF_LINE_SELECT in circuit_config:
            default_name = f"{_circuit_default_base_name(circuit_key, circuit_config)} Line"
            line_select_config = circuit_config.get(CONF_LINE_SELECT)
            if line_select_config is None or line_select_config is True:
                line_select_config = {CONF_NAME: _default_entity_name(default_name)}
                name_is_default = True
            elif not isinstance(line_select_config, dict):
                raise cv.Invalid(f"circuits.{circuit_key}.line_select must be a mapping")
            else:
                line_select_config = dict(line_select_config)
                name_is_default = line_select_config.get(CONF_NAME) in (None, default_name)
                if CONF_NAME not in line_select_config:
                    line_select_config[CONF_NAME] = _default_entity_name(default_name)
            if prefix and name_is_default:
                line_select_config[CONF_NAME] = _prefixed_entity_name(
                    prefix, _default_entity_name(default_name)
                )
            circuit_config[CONF_LINE_SELECT] = line_select_config

        if CONF_PHASE_DETECTION not in circuit_config:
            circuits[circuit_key] = circuit_config
            continue

        phase_detection_config = circuit_config[CONF_PHASE_DETECTION]
        if phase_detection_config is False or phase_detection_config is None:
            circuit_config = dict(circuit_config)
            circuit_config.pop(CONF_PHASE_DETECTION, None)
            circuits[circuit_key] = circuit_config
            continue
        if phase_detection_config is True:
            phase_detection_config = {}
        elif not isinstance(phase_detection_config, dict):
            raise cv.Invalid(f"circuits.{circuit_key}.phase_detection must be true or a mapping")
        else:
            phase_detection_config = dict(phase_detection_config)

        default_name = _circuit_default_phase_name(circuit_key, circuit_config)
        name_is_default = phase_detection_config.get(CONF_NAME) in (None, default_name)
        if phase_detection_config.get(CONF_NAME) is None:
            phase_detection_config[CONF_NAME] = _default_entity_name(default_name)
        if prefix and name_is_default:
            phase_detection_config[CONF_NAME] = _prefixed_entity_name(
                prefix, _default_entity_name(default_name)
            )
        phase_detection_config[CONF_ENTITY_CATEGORY] = ENTITY_CATEGORY_DIAGNOSTIC

        circuit_config = dict(circuit_config)
        circuit_config[CONF_PHASE_DETECTION] = phase_detection_config
        circuits[circuit_key] = circuit_config

    config[CONF_CIRCUITS] = circuits
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


def _apply_forced_entity_prefix(config):
    config = dict(config)
    prefix = config.get(CONF_ENTITY_PREFIX, "")
    if not prefix or not config.get(CONF_FORCE_ENTITY_PREFIX, False):
        return config

    def prefix_names(value):
        if isinstance(value, dict):
            value = dict(value)
            name = value.get(CONF_NAME)
            if isinstance(name, str):
                value[CONF_NAME] = _prefixed_entity_name(prefix, name)
            for key, child in list(value.items()):
                value[key] = prefix_names(child)
            return value
        if isinstance(value, list):
            return [prefix_names(child) for child in value]
        return value

    return prefix_names(config)


def _apply_hardware_defaults(config):
    config = dict(config)
    if config.get(CONF_HARDWARE) == HARDWARE_VUE2:
        config.setdefault(CONF_SWDIO_PIN, "GPIO13")
        config.setdefault(CONF_SWCLK_PIN, "GPIO14")
        config.setdefault(CONF_RESET_PIN, "GPIO26")
        config.setdefault(CONF_CONNECT_UNDER_RESET, True)
        config.setdefault(CONF_RESET_RELEASE_TIME, "1ms")
        config.setdefault(CONF_CLOCK_DELAY, 2)
    return config


def _apply_defaults(config):
    return _apply_forced_entity_prefix(
        _apply_entity_name_defaults(
            _apply_diagnostics_defaults(
                _apply_filter_defaults(
                    _apply_phase_detection_defaults(
                        _apply_virtual_line_defaults(
                            _apply_raw_power_defaults(
                                _apply_mains_defaults(
                                    _apply_external_firmware_defaults(_apply_hardware_defaults(config))
                                )
                            )
                        )
                    )
                )
            )
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


def _bytes_to_c_array(data, indent="    "):
    rows = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        rows.append(indent + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return "\n".join(rows)


def _empty_bundled_samd_firmware_header():
    return """#pragma once

#include <cstdint>

namespace esphome {
namespace emporiavue {

static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_HARDWARE_ID = 0UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_MODE_ID = 0UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_VERSION = 0UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_SIZE = 0UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_SOURCE_SIZE = 0UL;
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE_SHA256[32] = {0};
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE_PAYLOAD_SHA256[32] = {0};
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE[1] = {0};

}  // namespace emporiavue
}  // namespace esphome
"""


def _read_bundled_samd_firmware_image(path):
    data = path.read_bytes()
    footer_size = struct.calcsize(BUNDLED_SAMD_FIRMWARE_FOOTER_FORMAT)
    if len(data) <= footer_size:
        raise cv.Invalid(f"bundled SAMD firmware image is too small: {path}")
    hardware_id, mode_id, version, payload_sha, marker, _reserved = struct.unpack(
        BUNDLED_SAMD_FIRMWARE_FOOTER_FORMAT, data[-footer_size:]
    )
    if marker != BUNDLED_SAMD_FIRMWARE_MARKER:
        raise cv.Invalid(f"bundled SAMD firmware image has no valid footer: {path}")
    payload = data[:-footer_size]
    expected_payload_sha = hashlib.sha256(payload).digest()
    if expected_payload_sha != payload_sha:
        raise cv.Invalid(f"bundled SAMD firmware payload hash mismatch: {path}")
    source_size = len(payload.rstrip(b"\xff"))
    return {
        "path": path,
        "data": data,
        "hardware_id": hardware_id,
        "mode_id": mode_id,
        "version": version,
        "source_size": source_size,
        "image_sha": hashlib.sha256(data).digest(),
        "payload_sha": payload_sha,
    }


def _selected_bundled_samd_firmware(config):
    hardware_slug = HARDWARE_SLUGS.get(config[CONF_HARDWARE])
    mode_slug = MODE_SLUGS[config[CONF_MODE]]
    expected_hardware_id = HARDWARE_IDS[config[CONF_HARDWARE]]
    expected_mode_id = FIRMWARE_MODE_IDS[config[CONF_MODE]]
    if hardware_slug is None:
        return None

    image_dir = Path(__file__).resolve().parents[2] / "firmware" / "samd09" / "images" / mode_slug
    candidates = sorted(image_dir.glob(f"{hardware_slug}-{mode_slug}-v*.bin"))
    if not candidates:
        return None

    images = [_read_bundled_samd_firmware_image(candidate) for candidate in candidates]
    valid_images = [
        image
        for image in images
        if image["hardware_id"] == expected_hardware_id and image["mode_id"] == expected_mode_id
    ]
    if not valid_images:
        raise cv.Invalid(f"no bundled SAMD firmware image matches {hardware_slug} {mode_slug}")
    return max(valid_images, key=lambda image: image["version"])


def _bundled_samd_firmware_header(config):
    firmware = _selected_bundled_samd_firmware(config)
    if firmware is None:
        return _empty_bundled_samd_firmware_header()

    return f"""#pragma once

#include <cstdint>

namespace esphome {{
namespace emporiavue {{

static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_HARDWARE_ID = {firmware["hardware_id"]}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_MODE_ID = {firmware["mode_id"]}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_VERSION = {firmware["version"]}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_SIZE = {len(firmware["data"])}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_SOURCE_SIZE = {firmware["source_size"]}UL;
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE_SHA256[32] = {{
{_bytes_to_c_array(firmware["image_sha"])}
}};
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE_PAYLOAD_SHA256[32] = {{
{_bytes_to_c_array(firmware["payload_sha"])}
}};

static constexpr uint8_t BUNDLED_SAMD_FIRMWARE[BUNDLED_SAMD_FIRMWARE_SIZE] = {{
{_bytes_to_c_array(firmware["data"])}
}};

}}  // namespace emporiavue
}}  // namespace esphome
"""


def _write_bundled_samd_firmware_header(config):
    try:
        BUNDLED_SAMD_FIRMWARE_HEADER.write_text(
            _bundled_samd_firmware_header(config), encoding="utf-8"
        )
    except OSError as err:
        raise cv.Invalid(f"bundled SAMD firmware header generation failed: {err}") from err


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
        body = _bytes_to_c_array(data)
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


VOLTAGE_CALIBRATION_NUMBER_SCHEMA = number.number_schema(
    MeteringCalibrationNumber,
    icon="mdi:tune",
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.Optional(CONF_INITIAL_VALUE): cv.positive_float,
    }
)

CURRENT_GAIN_NUMBER_SCHEMA = number.number_schema(
    MeteringCurrentGainNumber,
    icon="mdi:tune",
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.Optional(CONF_INITIAL_VALUE): cv.float_range(min=0.5, max=2.0),
    }
)

CURRENT_PHASE_NUMBER_SCHEMA = number.number_schema(
    MeteringCurrentPhaseNumber,
    unit_of_measurement=UNIT_DEGREES,
    icon="mdi:angle-acute",
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.Optional(CONF_INITIAL_VALUE): cv.All(cv.angle, cv.float_range(min=-10.0, max=10.0)),
    }
)

CURRENT_CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_GAIN, default=1.0): cv.float_range(min=0.5, max=2.0),
        cv.Optional(CONF_PHASE, default=0.0): cv.All(cv.angle, cv.float_range(min=-10.0, max=10.0)),
        cv.Optional(CONF_GAIN_NUMBER): CURRENT_GAIN_NUMBER_SCHEMA,
        cv.Optional(CONF_PHASE_NUMBER): CURRENT_PHASE_NUMBER_SCHEMA,
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
    accuracy_decimals=2,
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

APPARENT_POWER_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="VA",
    device_class="apparent_power",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

POWER_FACTOR_SENSOR_SCHEMA = sensor.sensor_schema(
    icon="mdi:cosine-wave",
    device_class="power_factor",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)

FUNDAMENTAL_REACTIVE_POWER_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="var",
    device_class="reactive_power",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

DISPLACEMENT_ANGLE_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon="mdi:angle-acute",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)

CURRENT_THD_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="%",
    icon="mdi:sine-wave",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

VOLTAGE_THD_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="%",
    icon="mdi:sine-wave",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=1,
)

CURRENT_PEAK_SENSOR_SCHEMA = CURRENT_SENSOR_SCHEMA

CURRENT_CREST_FACTOR_SENSOR_SCHEMA = sensor.sensor_schema(
    icon="mdi:pulse",
    state_class=STATE_CLASS_MEASUREMENT,
    accuracy_decimals=2,
)

POWER_DEMAND_SENSOR_SCHEMA = POWER_SENSOR_SCHEMA
CURRENT_DEMAND_SENSOR_SCHEMA = CURRENT_SENSOR_SCHEMA

MAXIMUM_POWER_DEMAND_SENSOR_SCHEMA = POWER_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_RESTORE, default=True): cv.boolean,
    }
)

MAXIMUM_CURRENT_DEMAND_SENSOR_SCHEMA = CURRENT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_RESTORE, default=True): cv.boolean,
    }
)

POWER_SPLIT_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional("line_1"): POWER_SENSOR_SCHEMA,
        cv.Optional("line_2"): POWER_SENSOR_SCHEMA,
        cv.Optional("line_3"): POWER_SENSOR_SCHEMA,
    }
)

FILTER_DEFAULT_VALUE_SCHEMA = cv.ensure_list(dict)

FILTER_DEFAULTS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_FREQUENCY): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_PHASE_ANGLE): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_CURRENT): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_POWER): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_POWER_APPARENT): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_POWER_FACTOR): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_CURRENT): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_REACTIVE_POWER): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_POWER_FACTOR): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_DISPLACEMENT_ANGLE): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_CURRENT_THD): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_VOLTAGE_THD): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_POWER_DEMAND): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_MAXIMUM_POWER_DEMAND): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_CURRENT_DEMAND): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_MAXIMUM_CURRENT_DEMAND): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_CURRENT_PEAK): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_CURRENT_CREST_FACTOR): FILTER_DEFAULT_VALUE_SCHEMA,
        cv.Optional(CONF_ENERGY): FILTER_DEFAULT_VALUE_SCHEMA,
    }
)


ENERGY_SENSOR_SCHEMA = sensor.sensor_schema(
    TotalDailyEnergy,
    unit_of_measurement="kWh",
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    accuracy_decimals=2,
).extend(
    {
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_RESTORE, default=True): cv.boolean,
        cv.Optional(CONF_METHOD, default="left"): cv.enum(TOTAL_DAILY_ENERGY_METHODS, lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_energy_sensor(value):
    if value is True or value is None:
        value = {}
    elif value is False:
        return None
    elif not isinstance(value, dict):
        raise cv.Invalid("energy must be true, false, empty, or a mapping")
    return ENERGY_SENSOR_SCHEMA(value)


POWER_OUTPUT_SENSOR_SCHEMA = POWER_SENSOR_SCHEMA.extend(
    {
        cv.Optional(CONF_DIRECTION, default=DIRECTION_BOTH): cv.one_of(
            DIRECTION_BOTH, DIRECTION_SIGNED_ALIAS, DIRECTION_POSITIVE, DIRECTION_NEGATIVE, lower=True
        ),
        cv.Optional(CONF_RAW_POWER): POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_ENERGY): _validate_energy_sensor,
    }
)


def _validate_power_outputs(value):
    return cv.ensure_list(POWER_OUTPUT_SENSOR_SCHEMA)(_normalize_power_output_list(value, "power"))


def _validate_watts(value):
    if isinstance(value, str):
        normalized = value.strip().lower().replace(" ", "")
        multiplier = 1.0
        if normalized.endswith("kw"):
            normalized = normalized[:-2]
            multiplier = 1000.0
        elif normalized.endswith("w"):
            normalized = normalized[:-1]
        try:
            value = float(normalized) * multiplier
        except ValueError as err:
            raise cv.Invalid("power value must be a number, W, or kW value") from err
    value = cv.float_(value)
    if value <= 0:
        raise cv.Invalid("power value must be positive")
    return value


def _validate_volt_amps(value):
    if isinstance(value, str):
        normalized = value.strip().lower().replace(" ", "")
        multiplier = 1.0
        if normalized.endswith("kva"):
            normalized = normalized[:-3]
            multiplier = 1000.0
        elif normalized.endswith("va"):
            normalized = normalized[:-2]
        try:
            value = float(normalized) * multiplier
        except ValueError as err:
            raise cv.Invalid("apparent power value must be a number, VA, or kVA value") from err
    value = cv.float_(value)
    if value < 0:
        raise cv.Invalid("apparent power value must not be negative")
    return value


def _validate_amperes(value):
    if isinstance(value, str):
        normalized = value.strip().lower().replace(" ", "")
        multiplier = 1.0
        if normalized.endswith("ma"):
            normalized = normalized[:-2]
            multiplier = 0.001
        elif normalized.endswith("a"):
            normalized = normalized[:-1]
        try:
            value = float(normalized) * multiplier
        except ValueError as err:
            raise cv.Invalid("current value must be a number, A, or mA value") from err
    value = cv.float_(value)
    if value <= 0:
        raise cv.Invalid("current value must be positive")
    return value


def _validate_demand_interval(value):
    value = cv.positive_time_period_milliseconds(value)
    milliseconds = value.total_milliseconds
    if milliseconds < 60000 or milliseconds > 3600000:
        raise cv.Invalid("demand_interval must be between 1min and 60min")
    if milliseconds % 5000 != 0:
        raise cv.Invalid("demand_interval must be a multiple of 5s")
    return value


def _validate_peak_interval(value):
    value = cv.positive_time_period_milliseconds(value)
    milliseconds = value.total_milliseconds
    if milliseconds < 1000 or milliseconds > 60000:
        raise cv.Invalid("peak_interval must be between 1s and 60s")
    return value


def _validate_confidence_ratio(value):
    value = cv.float_(value)
    if value <= 1.0:
        raise cv.Invalid("confidence_ratio must be greater than 1.0")
    return value


def _validate_gpio_number(value):
    if isinstance(value, str):
        normalized = value.strip().upper()
        if normalized.startswith("GPIO"):
            normalized = normalized[4:]
        value = normalized
    return cv.int_range(min=0, max=48)(value)

INTERNAL_POWER_FILTER_SCHEMA = cv.ensure_list(
    cv.Any(
        cv.Schema({cv.Required(CONF_MULTIPLY): cv.float_}),
        cv.Schema({cv.Required(CONF_LAMBDA): cv.returning_lambda}),
    )
)

PHASE_DETECTION_GLOBAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POWER_MIN, default=30.0): _validate_watts,
        cv.Optional(CONF_CONFIDENCE_RATIO, default=1.5): _validate_confidence_ratio,
        cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.positive_time_period_milliseconds,
    }
)

PHASE_DETECTION_SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    icon="mdi:transmission-tower",
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.Optional(CONF_POWER_MIN): _validate_watts,
    }
)

LINE_SELECT_SCHEMA = select.select_schema(
    MeteringLineSelect,
    icon="mdi:transmission-tower",
    entity_category=ENTITY_CATEGORY_CONFIG,
)


def _validate_phase_detection_sensor(value):
    if value is True:
        value = {}
    elif value is False or value is None:
        return None
    elif not isinstance(value, dict):
        raise cv.Invalid("phase_detection must be true or a mapping")
    return PHASE_DETECTION_SENSOR_SCHEMA(value)


def _validate_metering_phases(value):
    phases = cv.Schema(
        cv.ensure_list(
            {
                cv.Required(CONF_ID): cv.declare_id(MeteringPhaseConfig),
                cv.Required(CONF_INPUT): cv.one_of(*PHASE_INPUTS.keys(), upper=True),
                cv.Optional(CONF_VOLTAGE_CALIBRATION, default=0.022): cv.positive_float,
                cv.Optional(CONF_VOLTAGE_CALIBRATION_NUMBER): VOLTAGE_CALIBRATION_NUMBER_SCHEMA,
                cv.Optional(CONF_VOLTAGE): PHASE_VOLTAGE_SENSOR_SCHEMA,
                cv.Optional(CONF_FREQUENCY): PHASE_FREQUENCY_SENSOR_SCHEMA,
                cv.Optional(CONF_PHASE_ANGLE): PHASE_ANGLE_SENSOR_SCHEMA,
                cv.Optional(CONF_VOLTAGE_THD): VOLTAGE_THD_SENSOR_SCHEMA,
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
        if CONF_VOLTAGE_CALIBRATION_NUMBER in phase:
            calibration_number_config = dict(phase[CONF_VOLTAGE_CALIBRATION_NUMBER])
            calibration_number_config.setdefault(CONF_INITIAL_VALUE, phase[CONF_VOLTAGE_CALIBRATION])
            phase[CONF_VOLTAGE_CALIBRATION_NUMBER] = calibration_number_config
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
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Required(CONF_VOLTAGE_CALIBRATION): cv.positive_float,
        cv.Optional(CONF_VOLTAGE_CALIBRATION_NUMBER): VOLTAGE_CALIBRATION_NUMBER_SCHEMA,
        cv.Optional(CONF_CURRENT_CALIBRATION): CURRENT_CALIBRATION_SCHEMA,
        cv.Optional(CONF_VOLTAGE): PHASE_VOLTAGE_SENSOR_SCHEMA,
        cv.Optional(CONF_FREQUENCY): PHASE_FREQUENCY_SENSOR_SCHEMA,
        cv.Optional(CONF_PHASE_ANGLE): PHASE_ANGLE_SENSOR_SCHEMA,
        cv.Optional(CONF_VOLTAGE_THD): VOLTAGE_THD_SENSOR_SCHEMA,
        cv.Optional(CONF_FILTERS): INTERNAL_POWER_FILTER_SCHEMA,
        cv.Optional(CONF_POWER): _validate_power_outputs,
        cv.Optional(CONF_CURRENT): CURRENT_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_APPARENT): APPARENT_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_CURRENT): CURRENT_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_REACTIVE_POWER): FUNDAMENTAL_REACTIVE_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_DISPLACEMENT_ANGLE): DISPLACEMENT_ANGLE_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_THD): CURRENT_THD_SENSOR_SCHEMA,
        cv.Optional(CONF_PEAK_INTERVAL): _validate_peak_interval,
        cv.Optional(CONF_CURRENT_PEAK): CURRENT_PEAK_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_CREST_FACTOR): CURRENT_CREST_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_DEMAND_INTERVAL): _validate_demand_interval,
        cv.Optional(CONF_POWER_DEMAND): POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_POWER_DEMAND): MAXIMUM_POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_DEMAND): CURRENT_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_CURRENT_DEMAND): MAXIMUM_CURRENT_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_ENERGY): _validate_energy_sensor,
    }
)


def _validate_metering_line(value):
    if isinstance(value, str) and value.lower() == "auto":
        return "auto"
    if not isinstance(value, list):
        return cv.int_range(min=1, max=3)(value)

    lines = [cv.int_range(min=1, max=3)(line) for line in value]
    if len(lines) != 2:
        raise cv.Invalid("Line-to-line circuits need exactly two line numbers")
    if lines[0] == lines[1]:
        raise cv.Invalid("Line-to-line circuits need two different line numbers")
    return lines


def _validate_virtual_line_lines(value):
    if not isinstance(value, list):
        raise cv.Invalid("virtual_lines entries need lines: [line_a, line_b]")

    lines = [cv.int_range(min=1, max=3)(line) for line in value]
    if len(lines) != 2:
        raise cv.Invalid("virtual_lines entries need exactly two line numbers")
    if lines[0] == lines[1]:
        raise cv.Invalid("virtual_lines entries need two different line numbers")
    return lines


def _metering_line_numbers(value):
    if isinstance(value, list):
        return value
    if value == "auto":
        return []
    return [value]


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
        cv.Required(CONF_LINE): _validate_metering_line,
        cv.Optional(CONF_LINE_SELECT): LINE_SELECT_SCHEMA,
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_CURRENT_CALIBRATION): CURRENT_CALIBRATION_SCHEMA,
        cv.Optional(CONF_FILTERS): INTERNAL_POWER_FILTER_SCHEMA,
        cv.Optional(CONF_POWER): _validate_power_outputs,
        cv.Optional(CONF_CURRENT): CURRENT_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_APPARENT): APPARENT_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_CURRENT): CURRENT_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_REACTIVE_POWER): FUNDAMENTAL_REACTIVE_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_DISPLACEMENT_ANGLE): DISPLACEMENT_ANGLE_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_THD): CURRENT_THD_SENSOR_SCHEMA,
        cv.Optional(CONF_PEAK_INTERVAL): _validate_peak_interval,
        cv.Optional(CONF_CURRENT_PEAK): CURRENT_PEAK_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_CREST_FACTOR): CURRENT_CREST_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_DEMAND_INTERVAL): _validate_demand_interval,
        cv.Optional(CONF_POWER_DEMAND): POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_POWER_DEMAND): MAXIMUM_POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_DEMAND): CURRENT_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_CURRENT_DEMAND): MAXIMUM_CURRENT_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_SPLIT): POWER_SPLIT_SENSOR_SCHEMA,
        cv.Optional(CONF_PHASE_DETECTION): _validate_phase_detection_sensor,
        cv.Optional(CONF_ENERGY): _validate_energy_sensor,
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
        cv.Required(CONF_SOURCES): cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_SOURCES_TO_SUBDEVICE, default=False): cv.Any(
            cv.boolean,
            cv.one_of("all", lower=True),
            cv.ensure_list(cv.string_strict),
        ),
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_FILTERS): INTERNAL_POWER_FILTER_SCHEMA,
        cv.Optional(CONF_POWER): _validate_power_outputs,
        cv.Optional(CONF_DEMAND_INTERVAL): _validate_demand_interval,
        cv.Optional(CONF_POWER_DEMAND): POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_POWER_DEMAND): MAXIMUM_POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_ENERGY): _validate_energy_sensor,
    }
)


def _validate_groups(value):
    groups = cv.Schema({cv.string_strict: METERING_GROUP_SCHEMA})(value)

    for group_key, group_config in groups.items():
        sources = group_config[CONF_SOURCES]
        if not sources:
            raise cv.Invalid("Group needs at least one source", path=[group_key, CONF_SOURCES])
        normalized_sources = _normalize_group_sources(sources, [group_key, CONF_SOURCES])
        group_config[CONF_SOURCES] = normalized_sources

        source_keys = [_parse_group_source(source)[1] for source in normalized_sources]
        selected_sources = group_config[CONF_SOURCES_TO_SUBDEVICE]
        if selected_sources is True or selected_sources == "all":
            selected_sources = source_keys
        elif selected_sources is False:
            selected_sources = []
        else:
            selected_sources = list(dict.fromkeys(selected_sources))
            for source_key in selected_sources:
                if source_key not in source_keys:
                    raise cv.Invalid(
                        f"sources_to_subdevice references {source_key}, but it is not listed in sources",
                        path=[group_key, CONF_SOURCES_TO_SUBDEVICE],
                    )
        group_config[CONF_SOURCES_TO_SUBDEVICE] = selected_sources

    return groups


METERING_VIRTUAL_LINE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MeteringVirtualLineConfig),
        cv.Required(CONF_LINES): _validate_virtual_line_lines,
        cv.Required(CONF_VOLTAGE): PHASE_VOLTAGE_SENSOR_SCHEMA,
    }
)


def _validate_virtual_lines(value):
    return cv.Schema({cv.string_strict: METERING_VIRTUAL_LINE_SCHEMA})(value)


def _validate_metering_topology(config):
    circuits = config.get(CONF_CIRCUITS, {})
    mains = config.get(CONF_MAINS, {})
    groups = config.get(CONF_GROUPS, {})
    source_keys = set(circuits.keys()) | set(mains.keys()) | set(groups.keys())

    if config[CONF_MODE] != MODE_SPI:
        analysis_nodes = [
            *((f"mains.{key}", node) for key, node in mains.items()),
            *((f"circuits.{key}", node) for key, node in circuits.items()),
            *((f"phases[{index}]", node) for index, node in enumerate(config.get(CONF_PHASES, []))),
            *((f"ct_clamps[{index}]", node) for index, node in enumerate(config.get(CONF_CT_CLAMPS, []))),
        ]
        for path, node in analysis_nodes:
            for key in SPI_ANALYSIS_SENSOR_KEYS:
                if key in node:
                    raise cv.Invalid(f"{path}.{key} is only supported in SPI mode")
            current_calibration = node.get(CONF_CURRENT_CALIBRATION)
            if current_calibration and (
                current_calibration.get(CONF_PHASE, 0.0) != 0.0
                or CONF_PHASE_NUMBER in current_calibration
            ):
                raise cv.Invalid(
                    f"{path}.current_calibration.phase is only supported in SPI mode"
                )

    for circuit_key, circuit_config in circuits.items():
        if (
            circuit_config[CONF_LINE] == "auto" or CONF_LINE_SELECT in circuit_config
        ) and len(mains) < 2:
            raise cv.Invalid(
                f"circuits.{circuit_key} automatic line selection needs at least two configured mains lines"
            )
        if CONF_LINE_SELECT in circuit_config and isinstance(circuit_config[CONF_LINE], list):
            raise cv.Invalid(
                f"circuits.{circuit_key}.line_select is only available for single-line circuits"
            )
        if CONF_PHASE_DETECTION in circuit_config and isinstance(circuit_config[CONF_LINE], list):
            raise cv.Invalid(
                f"circuits.{circuit_key}.phase_detection is only supported for single-line circuits"
            )
        if CONF_POWER_SPLIT in circuit_config:
            if not isinstance(circuit_config[CONF_LINE], list):
                raise cv.Invalid(
                    f"circuits.{circuit_key}.power_split is only supported for line-to-line circuits"
                )
            allowed_keys = {
                _power_split_line_key(line_number)
                for line_number in circuit_config[CONF_LINE]
            }
            for line_key in circuit_config[CONF_POWER_SPLIT]:
                if line_key not in allowed_keys:
                    allowed = ", ".join(sorted(allowed_keys))
                    raise cv.Invalid(
                        f"circuits.{circuit_key}.power_split.{line_key} does not match line: "
                        f"{circuit_config[CONF_LINE]}; use {allowed}"
                    )
        for line_number in _metering_line_numbers(circuit_config[CONF_LINE]):
            line_key = f"line_{line_number}"
            if line_key not in mains:
                raise cv.Invalid(
                    f"circuits.{circuit_key}.line references {line_key}, but mains.{line_key} is not configured"
                )

    for group_key, group_config in groups.items():
        for source in group_config[CONF_SOURCES]:
            _, source_key = _parse_group_source(source)
            if source_key not in source_keys:
                raise cv.Invalid(
                    f"groups.{group_key}.sources references {source_key}, but no matching main, circuit, or group is configured"
                )
            if source_key == group_key:
                raise cv.Invalid(f"groups.{group_key}.sources must not reference itself")

    if config[CONF_ESPHOME_SUBDEVICES]:
        source_subdevice_groups = {}
        for group_key, group_config in groups.items():
            for source_key in group_config[CONF_SOURCES_TO_SUBDEVICE]:
                existing_group = source_subdevice_groups.get(source_key)
                if existing_group is not None and existing_group != group_key:
                    raise cv.Invalid(
                        f"groups.{group_key}.sources_to_subdevice cannot include {source_key}; "
                        f"it is already assigned to groups.{existing_group}"
                    )
                source_subdevice_groups[source_key] = group_key

    group_dependencies = {
        group_key: [
            _parse_group_source(source)[1]
            for source in group_config[CONF_SOURCES]
            if _parse_group_source(source)[1] in groups
        ]
        for group_key, group_config in groups.items()
    }
    visiting = set()
    visited = set()

    def visit_group(group_key):
        if group_key in visited:
            return
        if group_key in visiting:
            raise cv.Invalid(f"groups.{group_key}.sources contains a group reference cycle")
        visiting.add(group_key)
        for dependency in group_dependencies.get(group_key, []):
            visit_group(dependency)
        visiting.remove(group_key)
        visited.add(group_key)

    for group_key in groups:
        visit_group(group_key)

    for virtual_line_key, virtual_line_config in config.get(CONF_VIRTUAL_LINES, {}).items():
        for line_number in virtual_line_config[CONF_LINES]:
            line_key = f"line_{line_number}"
            if line_key not in mains:
                raise cv.Invalid(
                    f"virtual_lines.{virtual_line_key}.lines references {line_key}, "
                    f"but mains.{line_key} is not configured"
                )

    return config


def _power_output_entity_configs(node_config):
    entity_configs = []

    for power_config in node_config.get(CONF_POWER, []):
        if not isinstance(power_config, dict):
            continue
        entity_configs.append(power_config)
        energy_config = power_config.get(CONF_ENERGY)
        if isinstance(energy_config, dict):
            entity_configs.append(energy_config)

    return entity_configs


def _current_calibration_entity_configs(node_config):
    current_calibration = node_config.get(CONF_CURRENT_CALIBRATION)
    if not isinstance(current_calibration, dict):
        return []
    entity_configs = []
    for key in (CONF_GAIN_NUMBER, CONF_PHASE_NUMBER):
        entity_config = current_calibration.get(key)
        if isinstance(entity_config, dict):
            entity_configs.append(entity_config)
    return entity_configs


def _main_entity_configs(main_config):
    entity_configs = _power_output_entity_configs(main_config)
    for key in MAIN_DIRECT_ENTITY_KEYS:
        entity_config = main_config.get(key)
        if isinstance(entity_config, dict):
            entity_configs.append(entity_config)
    entity_configs.extend(_current_calibration_entity_configs(main_config))
    return entity_configs


def _circuit_entity_configs(circuit_config):
    entity_configs = _power_output_entity_configs(circuit_config)

    for key in CIRCUIT_DIRECT_ENTITY_KEYS:
        entity_config = circuit_config.get(key)
        if isinstance(entity_config, dict):
            entity_configs.append(entity_config)

    power_split_config = circuit_config.get(CONF_POWER_SPLIT)
    if isinstance(power_split_config, dict):
        entity_configs.extend(
            entity_config
            for entity_config in power_split_config.values()
            if isinstance(entity_config, dict)
        )

    entity_configs.extend(_current_calibration_entity_configs(circuit_config))

    return entity_configs


def _group_entity_configs(group_config):
    entity_configs = _power_output_entity_configs(group_config)
    for key in (CONF_POWER_DEMAND, CONF_MAXIMUM_POWER_DEMAND):
        entity_config = group_config.get(key)
        if isinstance(entity_config, dict):
            entity_configs.append(entity_config)
    return entity_configs


def _relativize_default_entity_name(entity_config, device_name):
    name = entity_config.get(CONF_NAME)
    if not isinstance(name, _DefaultEntityName):
        return False

    device_prefix = f"{device_name} "
    if name.startswith(device_prefix):
        relative_name = name[len(device_prefix) :]
        if relative_name:
            entity_config[CONF_NAME] = _default_entity_name(relative_name)
    else:
        today_prefix = f"Today's {device_name} "
        if name.startswith(today_prefix):
            relative_name = name[len(today_prefix) :]
            if relative_name:
                entity_config[CONF_NAME] = _default_entity_name(
                    f"Today's {relative_name}"
                )
    return True


def _final_validate_esphome_subdevices(config):
    if not config[CONF_ESPHOME_SUBDEVICES]:
        return config

    full_config = fv.full_config.get()
    esphome_config = full_config.get_config_for_path([CONF_ESPHOME])
    devices = esphome_config[CONF_DEVICES]
    existing_device_ids = {str(device[CONF_ID]) for device in devices}
    component_id = str(config[CONF_ID])

    def visible_entities(entity_configs):
        return [
            entity_config
            for entity_config in entity_configs
            if not entity_config.get(CONF_INTERNAL, False)
            and CONF_DEVICE_ID not in entity_config
        ]

    def add_subdevice(node_type, device_id_suffix, name, target_entities):
        if not target_entities:
            return
        device_id = f"emporiavue_{component_id}_{device_id_suffix}"
        if device_id in existing_device_ids:
            raise cv.Invalid(
                f"generated {node_type} device ID {device_id} conflicts with esphome.devices"
            )
        devices.append(DEVICE_SCHEMA({CONF_ID: device_id, CONF_NAME: name}))
        existing_device_ids.add(device_id)

        device_reference = ID(device_id, type=Device)
        for entity_config in target_entities:
            entity_config[CONF_DEVICE_ID] = device_reference.copy()

    node_specs = {}
    for main_key, main_config in config.get(CONF_MAINS, {}).items():
        node_specs[main_key] = (
            "line",
            f"main_{main_key}",
            _main_default_base_name(main_key, main_config),
            _main_entity_configs(main_config),
        )
    for circuit_key, circuit_config in config.get(CONF_CIRCUITS, {}).items():
        node_specs[circuit_key] = (
            "circuit",
            circuit_key,
            _circuit_default_base_name(circuit_key, circuit_config),
            _circuit_entity_configs(circuit_config),
        )
    for group_key, group_config in config.get(CONF_GROUPS, {}).items():
        node_specs[group_key] = (
            "group",
            f"group_{group_key}",
            _group_default_base_name(group_key, group_config),
            _group_entity_configs(group_config),
        )

    source_owners = {}
    for group_key, group_config in config.get(CONF_GROUPS, {}).items():
        for source_key in group_config[CONF_SOURCES_TO_SUBDEVICE]:
            source_owners[source_key] = group_key

    def root_owner(node_key):
        while node_key in source_owners:
            node_key = source_owners[node_key]
        return node_key

    device_entities = {node_key: [] for node_key in node_specs}
    for node_key, (_, _, _, entity_configs) in node_specs.items():
        owner_key = root_owner(node_key)
        owner_name = node_specs[owner_key][2]
        for entity_config in visible_entities(entity_configs):
            _relativize_default_entity_name(entity_config, owner_name)
            device_entities[owner_key].append(entity_config)

    for node_key, (node_type, device_id_suffix, name, _) in node_specs.items():
        if root_owner(node_key) != node_key:
            continue
        add_subdevice(
            node_type,
            device_id_suffix,
            name,
            device_entities[node_key],
        )

    return config


METERING_CT_CLAMP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MeteringCTClampConfig),
        cv.Required(CONF_PHASE_ID): cv.use_id(MeteringPhaseConfig),
        cv.Required(CONF_INPUT): cv.one_of(*CT_INPUTS.keys(), upper=True),
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_CURRENT_CALIBRATION): CURRENT_CALIBRATION_SCHEMA,
        cv.Optional(CONF_FILTERS): INTERNAL_POWER_FILTER_SCHEMA,
        cv.Optional(CONF_POWER): _validate_power_outputs,
        cv.Optional(CONF_CURRENT): CURRENT_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_APPARENT): APPARENT_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_CURRENT): CURRENT_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_REACTIVE_POWER): FUNDAMENTAL_REACTIVE_POWER_SENSOR_SCHEMA,
        cv.Optional(CONF_FUNDAMENTAL_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_DISPLACEMENT_ANGLE): DISPLACEMENT_ANGLE_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_THD): CURRENT_THD_SENSOR_SCHEMA,
        cv.Optional(CONF_PEAK_INTERVAL): _validate_peak_interval,
        cv.Optional(CONF_CURRENT_PEAK): CURRENT_PEAK_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_CREST_FACTOR): CURRENT_CREST_FACTOR_SENSOR_SCHEMA,
        cv.Optional(CONF_DEMAND_INTERVAL): _validate_demand_interval,
        cv.Optional(CONF_POWER_DEMAND): POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_POWER_DEMAND): MAXIMUM_POWER_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_CURRENT_DEMAND): CURRENT_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_MAXIMUM_CURRENT_DEMAND): MAXIMUM_CURRENT_DEMAND_SENSOR_SCHEMA,
        cv.Optional(CONF_ENERGY): _validate_energy_sensor,
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
        cv.Optional(CONF_SPI_CLK_PIN, default="GPIO22"): _validate_gpio_number,
        cv.Optional(CONF_SPI_DATA_PIN, default="GPIO21"): _validate_gpio_number,
        cv.Optional(CONF_SPI_FRAME_PIN, default="GPIO13"): _validate_gpio_number,
        cv.Optional(CONF_SPI_MAIN_CURRENT_DELAY, default=2): cv.int_range(min=0, max=5),
        cv.Optional(CONF_SPI_MUX_CURRENT_DELAY, default=4): cv.int_range(min=0, max=5),
        cv.Optional(CONF_CONNECT_UNDER_RESET, default=False): cv.boolean,
        cv.Optional(CONF_SWD_ON_BOOT, default=True): cv.boolean,
        cv.Optional(CONF_RESET_RELEASE_TIME, default="50ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CLOCK_DELAY, default=2): cv.int_range(min=0, max=50),
        cv.Optional(CONF_MODE, default=MODE_I2C): cv.one_of(MODE_I2C, MODE_SPI, lower=True),
        cv.Optional(CONF_ENTITY_PREFIX): cv.string_strict,
        cv.Optional(CONF_FORCE_ENTITY_PREFIX, default=False): cv.boolean,
        cv.Optional(CONF_AUTO_UPDATE_SAMD, default=False): cv.boolean,
        cv.Optional(CONF_DIAGNOSTICS_INTERVAL): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_METERING_INTERVAL, default="220ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DEMAND_INTERVAL, default="15min"): _validate_demand_interval,
        cv.Optional(CONF_PEAK_INTERVAL, default="5s"): _validate_peak_interval,
        cv.Optional(CONF_MINIMUM_APPARENT_POWER, default="5VA"): _validate_volt_amps,
        cv.Optional(CONF_MINIMUM_FUNDAMENTAL_CURRENT, default="20mA"): _validate_amperes,
        cv.Optional(CONF_PHASE_DETECTION, default={}): PHASE_DETECTION_GLOBAL_SCHEMA,
        cv.Optional(CONF_FILTER_DEFAULTS): FILTER_DEFAULTS_SCHEMA,
        cv.Optional(CONF_MAINS): _validate_mains,
        cv.Optional(CONF_ESPHOME_SUBDEVICES, default=True): cv.boolean,
        cv.Optional(CONF_CIRCUITS): _validate_circuits,
        cv.Optional(CONF_GROUPS): _validate_groups,
        cv.Optional(CONF_VIRTUAL_LINES): _validate_virtual_lines,
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
        cv.Optional(CONF_DIAG_FRAME_ERRORS): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_TRANSFER_ERRORS): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_FRAME_OVERRUNS): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_RECOVERIES): sensor.sensor_schema(
            icon="mdi:backup-restore",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_LAST_FRAME_SAMPLES): sensor.sensor_schema(
            icon="mdi:pulse",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_SAMPLE_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            icon="mdi:speedometer",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_RESTART_REASON): text_sensor.text_sensor_schema(
            icon=ICON_RESTART,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_HEAP_FREE): sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_DATA_SIZE,
            icon="mdi:memory",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_HEAP_MINIMUM): sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_DATA_SIZE,
            icon="mdi:memory",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_LOOP_STACK_FREE): sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_DATA_SIZE,
            icon="mdi:memory",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_SPI_STACK_FREE): sensor.sensor_schema(
            unit_of_measurement=UNIT_BYTES,
            device_class=DEVICE_CLASS_DATA_SIZE,
            icon="mdi:memory",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_SPI_PROCESSING_LOAD): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:cpu-32-bit",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DIAG_SPI_PROCESSING_OVERRUNS): sensor.sensor_schema(
            icon="mdi:alert-circle-outline",
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
).extend(cv.COMPONENT_SCHEMA)

EMPORIAVUE_I2C_SCHEMA = EMPORIAVUE_SCHEMA.extend(i2c.i2c_device_schema(0x64))


def _validate_transport_schema(config):
    if config[CONF_MODE] == MODE_I2C:
        return EMPORIAVUE_I2C_SCHEMA(config)
    return EMPORIAVUE_SCHEMA(config)


CONFIG_SCHEMA = cv.All(
    _apply_defaults,
    _validate_transport_schema,
    _validate_metering_topology,
)

FINAL_VALIDATE_SCHEMA = _final_validate_esphome_subdevices


async def _add_internal_power_filters(var, filters):
    for filter_config in filters or []:
        if CONF_MULTIPLY in filter_config:
            cg.add(var.add_power_multiply_filter(filter_config[CONF_MULTIPLY]))
        elif CONF_LAMBDA in filter_config:
            lambda_ = await cg.process_lambda(
                filter_config[CONF_LAMBDA],
                [(cg.float_, "x")],
                return_type=cg.float_,
            )
            cg.add(var.add_power_lambda_filter(lambda_))


def _power_sensor_config_without_output_keys(config):
    config = dict(config)
    config.pop(CONF_ENERGY, None)
    config.pop(CONF_DIRECTION, None)
    config.pop(CONF_RAW_POWER, None)
    return config


async def _new_total_daily_energy_sensor(config, parent_sensor):
    energy_sensor = await sensor.new_sensor(config)
    await cg.register_component(energy_sensor, config)
    cg.add(energy_sensor.set_parent(parent_sensor))
    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(energy_sensor.set_time(time_))
    cg.add(energy_sensor.set_restore(config[CONF_RESTORE]))
    cg.add(energy_sensor.set_method(config[CONF_METHOD]))
    return energy_sensor


async def _add_power_outputs(var, power_configs):
    for power_config in power_configs or []:
        raw_sensor = await sensor.new_sensor(power_config[CONF_RAW_POWER])
        visible_sensor = await sensor.new_sensor(_power_sensor_config_without_output_keys(power_config))
        cg.add(
            var.add_power_output(
                POWER_DIRECTION_IDS[power_config[CONF_DIRECTION]],
                raw_sensor,
                visible_sensor,
            )
        )
        if energy_config := power_config.get(CONF_ENERGY):
            await _new_total_daily_energy_sensor(energy_config, raw_sensor)


async def _add_fundamental_analysis_sensors(var, config):
    sensor_setters = (
        (CONF_FUNDAMENTAL_CURRENT, var.set_fundamental_current_sensor),
        (CONF_FUNDAMENTAL_REACTIVE_POWER, var.set_fundamental_reactive_power_sensor),
        (CONF_FUNDAMENTAL_POWER_FACTOR, var.set_fundamental_power_factor_sensor),
        (CONF_DISPLACEMENT_ANGLE, var.set_displacement_angle_sensor),
        (CONF_CURRENT_THD, var.set_current_thd_sensor),
    )
    for key, setter in sensor_setters:
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(setter(sens))


async def _add_demand_sensors(var, config, default_interval, include_current=True):
    cg.add(var.set_demand_interval(config.get(CONF_DEMAND_INTERVAL, default_interval)))

    if sensor_config := config.get(CONF_POWER_DEMAND):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_power_demand_sensor(sens))
    if sensor_config := config.get(CONF_MAXIMUM_POWER_DEMAND):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_maximum_power_demand_sensor(sens))
        time_ = await cg.get_variable(sensor_config[CONF_TIME_ID])
        cg.add(var.set_power_demand_time(time_))
        cg.add(var.set_power_demand_restore(sensor_config[CONF_RESTORE]))

    if not include_current:
        return
    if sensor_config := config.get(CONF_CURRENT_DEMAND):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_current_demand_sensor(sens))
    if sensor_config := config.get(CONF_MAXIMUM_CURRENT_DEMAND):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_maximum_current_demand_sensor(sens))
        time_ = await cg.get_variable(sensor_config[CONF_TIME_ID])
        cg.add(var.set_current_demand_time(time_))
        cg.add(var.set_current_demand_restore(sensor_config[CONF_RESTORE]))


async def _add_peak_sensors(var, config, default_interval):
    cg.add(var.set_peak_interval(config.get(CONF_PEAK_INTERVAL, default_interval)))
    if sensor_config := config.get(CONF_CURRENT_PEAK):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_current_peak_sensor(sens))
    if sensor_config := config.get(CONF_CURRENT_CREST_FACTOR):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(var.set_current_crest_factor_sensor(sens))


def _metering_preference_key(metering_path):
    key = int.from_bytes(
        hashlib.sha256(
            f"emporiavue.preference.v1.{metering_path}".encode()
        ).digest()[:4],
        "little",
    )
    return key or 1


async def _add_current_calibration(var, config, metering_path):
    calibration = config.get(CONF_CURRENT_CALIBRATION)
    if not calibration:
        return
    cg.add(var.set_current_gain(calibration[CONF_GAIN]))
    cg.add(var.set_current_phase_correction(calibration[CONF_PHASE]))

    if number_config := calibration.get(CONF_GAIN_NUMBER):
        gain_number = await number.new_number(
            number_config,
            min_value=0.5,
            max_value=2.0,
            step=0.0001,
        )
        cg.add(gain_number.set_initial_value(number_config[CONF_INITIAL_VALUE]))
        cg.add(
            gain_number.set_preference_key(
                _metering_preference_key(f"{metering_path}.current_gain")
            )
        )
        await cg.register_parented(gain_number, var)
        cg.add(var.set_current_gain_number(gain_number))

    if number_config := calibration.get(CONF_PHASE_NUMBER):
        phase_number = await number.new_number(
            number_config,
            min_value=-10.0,
            max_value=10.0,
            step=0.01,
        )
        cg.add(phase_number.set_initial_value(number_config[CONF_INITIAL_VALUE]))
        cg.add(
            phase_number.set_preference_key(
                _metering_preference_key(f"{metering_path}.current_phase")
            )
        )
        await cg.register_parented(phase_number, var)
        cg.add(var.set_current_phase_number(phase_number))


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
    _write_bundled_samd_firmware_header(config)
    _write_external_samd_firmware_header(external_firmwares)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if config[CONF_MODE] == MODE_I2C:
        await i2c.register_i2c_device(var, config)
    cg.add(var.set_hardware_id(HARDWARE_IDS[config[CONF_HARDWARE]]))

    swdio_pin = await cg.gpio_pin_expression(config[CONF_SWDIO_PIN])
    cg.add(var.set_swdio_pin(swdio_pin))
    swclk_pin = await cg.gpio_pin_expression(config[CONF_SWCLK_PIN])
    cg.add(var.set_swclk_pin(swclk_pin))
    if reset_pin_config := config.get(CONF_RESET_PIN):
        reset_pin = await cg.gpio_pin_expression(reset_pin_config)
        cg.add(var.set_reset_pin(reset_pin))
    if config[CONF_MODE] == MODE_SPI:
        cg.add(var.set_spi_clk_pin(config[CONF_SPI_CLK_PIN]))
        cg.add(var.set_spi_data_pin(config[CONF_SPI_DATA_PIN]))
        cg.add(var.set_spi_frame_pin(config[CONF_SPI_FRAME_PIN]))
        cg.add(var.set_spi_main_current_delay(config[CONF_SPI_MAIN_CURRENT_DELAY]))
        cg.add(var.set_spi_mux_current_delay(config[CONF_SPI_MUX_CURRENT_DELAY]))

    cg.add(var.set_connect_under_reset(config[CONF_CONNECT_UNDER_RESET]))
    cg.add(var.set_swd_on_boot(config[CONF_SWD_ON_BOOT]))
    cg.add(var.set_reset_release_time(config[CONF_RESET_RELEASE_TIME]))
    cg.add(var.set_clock_delay_us(config[CONF_CLOCK_DELAY]))
    cg.add(var.set_runtime_mode(MODE_IDS[config[CONF_MODE]]))
    cg.add(var.set_entity_prefix(config.get(CONF_ENTITY_PREFIX, "")))
    cg.add(var.set_auto_update_samd(config[CONF_AUTO_UPDATE_SAMD]))
    if diagnostics_interval := config.get(CONF_DIAGNOSTICS_INTERVAL):
        cg.add(var.set_diagnostics_interval(diagnostics_interval))
    cg.add(var.set_metering_interval(config[CONF_METERING_INTERVAL]))
    cg.add(var.set_minimum_apparent_power(config[CONF_MINIMUM_APPARENT_POWER]))
    cg.add(var.set_minimum_fundamental_current(config[CONF_MINIMUM_FUNDAMENTAL_CURRENT]))
    phase_detection_config = config[CONF_PHASE_DETECTION]
    cg.add(var.set_phase_detection_confidence_ratio(phase_detection_config[CONF_CONFIDENCE_RATIO]))
    cg.add(var.set_phase_detection_update_interval(phase_detection_config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_backup_partition_name(config[CONF_BACKUP_PARTITION]))
    if firmware_version_config := config.get(CONF_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(firmware_version_config)
        cg.add(var.set_firmware_version_sensor(sens))
    if bundled_firmware_version_config := config.get(CONF_BUNDLED_FIRMWARE_VERSION):
        sens = await text_sensor.new_text_sensor(bundled_firmware_version_config)
        cg.add(var.set_bundled_firmware_version_sensor(sens))
    if diag_frame_errors_config := config.get(CONF_DIAG_FRAME_ERRORS):
        sens = await sensor.new_sensor(diag_frame_errors_config)
        cg.add(var.set_diag_frame_errors_sensor(sens))
    if diag_transfer_errors_config := config.get(CONF_DIAG_TRANSFER_ERRORS):
        sens = await sensor.new_sensor(diag_transfer_errors_config)
        cg.add(var.set_diag_transfer_errors_sensor(sens))
    if diag_frame_overruns_config := config.get(CONF_DIAG_FRAME_OVERRUNS):
        sens = await sensor.new_sensor(diag_frame_overruns_config)
        cg.add(var.set_diag_frame_overruns_sensor(sens))
    if diag_recoveries_config := config.get(CONF_DIAG_RECOVERIES):
        sens = await sensor.new_sensor(diag_recoveries_config)
        cg.add(var.set_diag_recoveries_sensor(sens))
    if diag_last_frame_samples_config := config.get(CONF_DIAG_LAST_FRAME_SAMPLES):
        sens = await sensor.new_sensor(diag_last_frame_samples_config)
        cg.add(var.set_diag_last_frame_samples_sensor(sens))
    if diag_sample_rate_config := config.get(CONF_DIAG_SAMPLE_RATE):
        sens = await sensor.new_sensor(diag_sample_rate_config)
        cg.add(var.set_diag_sample_rate_sensor(sens))
    if diag_restart_reason_config := config.get(CONF_DIAG_RESTART_REASON):
        sens = await text_sensor.new_text_sensor(diag_restart_reason_config)
        cg.add(var.set_diag_restart_reason_sensor(sens))
    if diag_heap_free_config := config.get(CONF_DIAG_HEAP_FREE):
        sens = await sensor.new_sensor(diag_heap_free_config)
        cg.add(var.set_diag_heap_free_sensor(sens))
    if diag_heap_minimum_config := config.get(CONF_DIAG_HEAP_MINIMUM):
        sens = await sensor.new_sensor(diag_heap_minimum_config)
        cg.add(var.set_diag_heap_minimum_sensor(sens))
    if diag_loop_stack_free_config := config.get(CONF_DIAG_LOOP_STACK_FREE):
        sens = await sensor.new_sensor(diag_loop_stack_free_config)
        cg.add(var.set_diag_loop_stack_free_sensor(sens))
    if diag_spi_stack_free_config := config.get(CONF_DIAG_SPI_STACK_FREE):
        sens = await sensor.new_sensor(diag_spi_stack_free_config)
        cg.add(var.set_diag_spi_stack_free_sensor(sens))
    if diag_spi_processing_load_config := config.get(CONF_DIAG_SPI_PROCESSING_LOAD):
        sens = await sensor.new_sensor(diag_spi_processing_load_config)
        cg.add(var.set_diag_spi_processing_load_sensor(sens))
    if diag_spi_processing_overruns_config := config.get(
        CONF_DIAG_SPI_PROCESSING_OVERRUNS
    ):
        sens = await sensor.new_sensor(diag_spi_processing_overruns_config)
        cg.add(var.set_diag_spi_processing_overruns_sensor(sens))

    phases = []
    ct_clamps = []
    main_phase_vars_by_line = {}
    power_source_ct_clamps_by_key = {}
    for phase_key, main_config in config.get(CONF_MAINS, {}).items():
        phase_var = cg.new_Pvariable(main_config[CONF_ID], MeteringPhaseConfig())
        cg.add(phase_var.set_config_key(phase_key))
        cg.add(phase_var.set_input_wire(PHASE_INPUTS[main_config[CONF_VOLTAGE_INPUT]]))
        cg.add(phase_var.set_calibration(main_config[CONF_VOLTAGE_CALIBRATION]))
        line_number = int(phase_key.rsplit("_", 1)[1])
        main_phase_vars_by_line[line_number] = phase_var

        if calibration_number_config := main_config.get(CONF_VOLTAGE_CALIBRATION_NUMBER):
            cal_num = await number.new_number(
                calibration_number_config,
                min_value=0.001,
                max_value=0.1,
                step=0.000001,
            )
            cg.add(cal_num.set_initial_value(calibration_number_config[CONF_INITIAL_VALUE]))
            cg.add(
                cal_num.set_preference_key(
                    _metering_preference_key(
                        f"mains.{phase_key}.voltage_calibration"
                    )
                )
            )
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
        if voltage_thd_config := main_config.get(CONF_VOLTAGE_THD):
            sens = await sensor.new_sensor(voltage_thd_config)
            cg.add(phase_var.set_voltage_thd_sensor(sens))

        phases.append(phase_var)

        ct_clamp_var = cg.new_Pvariable(main_config[CONF_CT_ID], MeteringCTClampConfig())
        cg.add(ct_clamp_var.set_config_key(phase_key))
        cg.add(ct_clamp_var.set_phase(phase_var))
        cg.add(
            ct_clamp_var.set_input_port(
                _main_ct_input_for_hardware(config[CONF_HARDWARE], main_config[CONF_MAIN_CLAMP])
            )
        )
        await _add_internal_power_filters(ct_clamp_var, main_config.get(CONF_FILTERS))
        await _add_current_calibration(ct_clamp_var, main_config, f"mains.{phase_key}")
        await _add_power_outputs(ct_clamp_var, main_config.get(CONF_POWER))
        if current_config := main_config.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(ct_clamp_var.set_current_sensor(sens))
        if apparent_power_config := main_config.get(CONF_POWER_APPARENT):
            sens = await sensor.new_sensor(apparent_power_config)
            cg.add(ct_clamp_var.set_apparent_power_sensor(sens))
        if power_factor_config := main_config.get(CONF_POWER_FACTOR):
            sens = await sensor.new_sensor(power_factor_config)
            cg.add(ct_clamp_var.set_power_factor_sensor(sens))
        await _add_fundamental_analysis_sensors(ct_clamp_var, main_config)
        await _add_demand_sensors(ct_clamp_var, main_config, config[CONF_DEMAND_INTERVAL])
        await _add_peak_sensors(ct_clamp_var, main_config, config[CONF_PEAK_INTERVAL])
        ct_clamps.append(ct_clamp_var)
        power_source_ct_clamps_by_key[phase_key] = ct_clamp_var

    for circuit_key, circuit_config in config.get(CONF_CIRCUITS, {}).items():
        ct_clamp_var = cg.new_Pvariable(circuit_config[CONF_CT_ID], MeteringCTClampConfig())
        cg.add(ct_clamp_var.set_config_key(circuit_key))
        line_config = circuit_config[CONF_LINE]
        if isinstance(line_config, list):
            phase_a_var = main_phase_vars_by_line[line_config[0]]
            phase_b_var = main_phase_vars_by_line[line_config[1]]
            cg.add(ct_clamp_var.set_line_pair(phase_a_var, phase_b_var))
        elif line_config != "auto":
            phase_var = main_phase_vars_by_line[line_config]
            cg.add(ct_clamp_var.set_phase(phase_var))
        cg.add(ct_clamp_var.set_input_port(BRANCH_CT_INPUTS[circuit_config[CONF_INPUT]]))
        await _add_internal_power_filters(ct_clamp_var, circuit_config.get(CONF_FILTERS))
        await _add_current_calibration(
            ct_clamp_var, circuit_config, f"circuits.{circuit_key}"
        )

        await _add_power_outputs(ct_clamp_var, circuit_config.get(CONF_POWER))
        if current_config := circuit_config.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(ct_clamp_var.set_current_sensor(sens))
        if apparent_power_config := circuit_config.get(CONF_POWER_APPARENT):
            sens = await sensor.new_sensor(apparent_power_config)
            cg.add(ct_clamp_var.set_apparent_power_sensor(sens))
        if power_factor_config := circuit_config.get(CONF_POWER_FACTOR):
            sens = await sensor.new_sensor(power_factor_config)
            cg.add(ct_clamp_var.set_power_factor_sensor(sens))
        await _add_fundamental_analysis_sensors(ct_clamp_var, circuit_config)
        await _add_demand_sensors(ct_clamp_var, circuit_config, config[CONF_DEMAND_INTERVAL])
        await _add_peak_sensors(ct_clamp_var, circuit_config, config[CONF_PEAK_INTERVAL])
        if power_split_config := circuit_config.get(CONF_POWER_SPLIT):
            line_a_key = _power_split_line_key(line_config[0])
            line_b_key = _power_split_line_key(line_config[1])
            if line_a_config := power_split_config.get(line_a_key):
                sens = await sensor.new_sensor(line_a_config)
                cg.add(ct_clamp_var.set_power_split_line_a_sensor(sens))
            if line_b_config := power_split_config.get(line_b_key):
                sens = await sensor.new_sensor(line_b_config)
                cg.add(ct_clamp_var.set_power_split_line_b_sensor(sens))
        is_auto_line = line_config == "auto"
        has_line_select = CONF_LINE_SELECT in circuit_config
        if is_auto_line or has_line_select:
            preference_key = _metering_preference_key(
                f"circuits.{circuit_key}.line"
            )
            initial_line = 0 if is_auto_line else line_config
            cg.add(ct_clamp_var.configure_dynamic_line(initial_line, preference_key))

        if has_line_select:
            line_options = ["Auto"] + [
                f"L{line_number}" for line_number in sorted(main_phase_vars_by_line)
            ]
            line_select = await select.new_select(
                circuit_config[CONF_LINE_SELECT], options=line_options
            )
            await cg.register_parented(line_select, ct_clamp_var)
            cg.add(ct_clamp_var.set_line_select(line_select))

        phase_detection_sensor_config = circuit_config.get(CONF_PHASE_DETECTION)
        if phase_detection_sensor_config:
            phase_detection_text_sensor_config = dict(phase_detection_sensor_config)
            phase_detection_text_sensor_config.pop(CONF_POWER_MIN, None)
            sens = await text_sensor.new_text_sensor(phase_detection_text_sensor_config)
            cg.add(ct_clamp_var.set_phase_detection_sensor(sens))
            cg.add(ct_clamp_var.set_phase_detection_name(phase_detection_sensor_config[CONF_NAME]))
            cg.add(
                ct_clamp_var.set_phase_detection_power_min(
                    phase_detection_sensor_config.get(
                        CONF_POWER_MIN, phase_detection_config[CONF_POWER_MIN]
                    )
                )
            )
        if is_auto_line or has_line_select or phase_detection_sensor_config:
            if not phase_detection_sensor_config:
                cg.add(
                    ct_clamp_var.set_phase_detection_power_min(
                        phase_detection_config[CONF_POWER_MIN]
                    )
                )
            for line_number, phase_var in sorted(main_phase_vars_by_line.items()):
                cg.add(ct_clamp_var.add_phase_detection_candidate(phase_var, line_number))

        ct_clamps.append(ct_clamp_var)
        power_source_ct_clamps_by_key[circuit_key] = ct_clamp_var

    virtual_lines = []
    for virtual_line_key, virtual_line_config in config.get(CONF_VIRTUAL_LINES, {}).items():
        virtual_line_var = cg.new_Pvariable(virtual_line_config[CONF_ID], MeteringVirtualLineConfig())
        cg.add(virtual_line_var.set_config_key(virtual_line_key))
        line_a, line_b = virtual_line_config[CONF_LINES]
        cg.add(virtual_line_var.set_lines(main_phase_vars_by_line[line_a], main_phase_vars_by_line[line_b]))
        voltage_sensor = await sensor.new_sensor(virtual_line_config[CONF_VOLTAGE])
        cg.add(virtual_line_var.set_voltage_sensor(voltage_sensor))
        virtual_lines.append(virtual_line_var)
    if virtual_lines:
        cg.add(var.set_metering_virtual_lines(virtual_lines))

    for phase_config in config.get(CONF_PHASES, []):
        phase_var = cg.new_Pvariable(phase_config[CONF_ID], MeteringPhaseConfig())
        cg.add(phase_var.set_config_key(str(phase_config[CONF_INPUT])))
        cg.add(phase_var.set_input_wire(PHASE_INPUTS[phase_config[CONF_INPUT]]))
        cg.add(phase_var.set_calibration(phase_config[CONF_VOLTAGE_CALIBRATION]))

        if calibration_number_config := phase_config.get(CONF_VOLTAGE_CALIBRATION_NUMBER):
            cal_num = await number.new_number(
                calibration_number_config,
                min_value=0.001,
                max_value=0.1,
                step=0.000001,
            )
            cg.add(cal_num.set_initial_value(calibration_number_config[CONF_INITIAL_VALUE]))
            cg.add(
                cal_num.set_preference_key(
                    _metering_preference_key(
                        f"legacy_phases.{phase_config[CONF_INPUT]}.voltage_calibration"
                    )
                )
            )
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
        if voltage_thd_config := phase_config.get(CONF_VOLTAGE_THD):
            sens = await sensor.new_sensor(voltage_thd_config)
            cg.add(phase_var.set_voltage_thd_sensor(sens))

        phases.append(phase_var)
    if phases:
        cg.add(var.set_metering_phases(phases))

    for ct_config in config.get(CONF_CT_CLAMPS, []):
        ct_clamp_var = cg.new_Pvariable(ct_config[CONF_ID], MeteringCTClampConfig())
        cg.add(ct_clamp_var.set_config_key(str(ct_config[CONF_INPUT])))
        phase_var = await cg.get_variable(ct_config[CONF_PHASE_ID])
        cg.add(ct_clamp_var.set_phase(phase_var))
        cg.add(ct_clamp_var.set_input_port(CT_INPUTS[ct_config[CONF_INPUT]]))
        await _add_internal_power_filters(ct_clamp_var, ct_config.get(CONF_FILTERS))
        await _add_current_calibration(
            ct_clamp_var, ct_config, f"legacy_ct_clamps.{ct_config[CONF_INPUT]}"
        )

        await _add_power_outputs(ct_clamp_var, ct_config.get(CONF_POWER))
        if current_config := ct_config.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(ct_clamp_var.set_current_sensor(sens))
        if apparent_power_config := ct_config.get(CONF_POWER_APPARENT):
            sens = await sensor.new_sensor(apparent_power_config)
            cg.add(ct_clamp_var.set_apparent_power_sensor(sens))
        if power_factor_config := ct_config.get(CONF_POWER_FACTOR):
            sens = await sensor.new_sensor(power_factor_config)
            cg.add(ct_clamp_var.set_power_factor_sensor(sens))
        await _add_fundamental_analysis_sensors(ct_clamp_var, ct_config)
        await _add_demand_sensors(ct_clamp_var, ct_config, config[CONF_DEMAND_INTERVAL])
        await _add_peak_sensors(ct_clamp_var, ct_config, config[CONF_PEAK_INTERVAL])

        ct_clamps.append(ct_clamp_var)
    if ct_clamps:
        cg.add(var.set_metering_ct_clamps(ct_clamps))

    groups = []
    group_vars_by_key = {}
    for group_key, group_config in config.get(CONF_GROUPS, {}).items():
        group_var = cg.new_Pvariable(group_config[CONF_ID], MeteringGroupConfig())
        cg.add(group_var.set_config_key(group_key))
        groups.append(group_var)
        group_vars_by_key[group_key] = group_var

    for group_key, group_config in config.get(CONF_GROUPS, {}).items():
        group_var = group_vars_by_key[group_key]
        for source in group_config[CONF_SOURCES]:
            sign, source_key = _parse_group_source(source)
            if source_key in power_source_ct_clamps_by_key:
                cg.add(group_var.add_ct_clamp_term(power_source_ct_clamps_by_key[source_key], sign))
            else:
                cg.add(group_var.add_group_term(group_vars_by_key[source_key], sign))
        await _add_internal_power_filters(group_var, group_config.get(CONF_FILTERS))
        await _add_power_outputs(group_var, group_config.get(CONF_POWER))
        await _add_demand_sensors(
            group_var, group_config, config[CONF_DEMAND_INTERVAL], include_current=False
        )
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
