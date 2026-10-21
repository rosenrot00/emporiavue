#!/usr/bin/env python3
"""Validate per-CT polarity and the topology-independent Vue 3 overlay.

Runtime P/Q, phase, direction, demand, magnitude and raw-sample regressions live
in test_spi_entities.py; detection direction is exercised in test_line_detection.py.
"""
import copy
import importlib.util

import yaml
from esphome.core import CORE

from test_esphome_metering import ROOT


def main():
    spec = importlib.util.spec_from_file_location("ev_polarity_test", ROOT / "components/emporiavue/__init__.py")
    component = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(component)
    for schema, base in [
        (component.METERING_MAIN_SCHEMA, {"voltage_input": "BLACK", "main_clamp": "A", "voltage_calibration": 0.022}),
        (component.METERING_CIRCUIT_SCHEMA, {"input": "1", "line": 1}),
        (component.METERING_CT_CLAMP_SCHEMA, {"input": "1", "phase_id": "line_a"}),
    ]:
        assert schema(base)["polarity"] == "normal"
        for value in ("normal", "inverted", "NORMAL", "INVERTED"):
            assert schema({**base, "polarity": value})["polarity"] == value.lower()
        for invalid in (True, False, 1, -1, None, "auto", "reverse", ""):
            try:
                schema({**base, "polarity": invalid})
            except component.cv.Invalid:
                pass
            else:
                raise AssertionError(f"Accepted invalid polarity: {invalid!r}")

    overlay = yaml.safe_load((ROOT / "packages/vue3-cir-inverted.yaml").read_text())["emporiavue"]
    assert set(overlay) == {"circuits"}, "Overlay must not add mains, entities or transport settings"
    assert overlay["circuits"] == {f"cir{i}": {"polarity": "inverted"} for i in range(1, 17)}
    for count in (1, 2, 3):
        CORE.reset()  # Independent nodes may use the same entity names.
        topology = yaml.safe_load((ROOT / f"packages/vue3-{count}phase.yaml").read_text())["emporiavue"]
        merged = copy.deepcopy(topology)
        for key, value in overlay["circuits"].items():
            merged["circuits"][key].update(value)
        for config in merged["mains"].values():
            assert component.METERING_MAIN_SCHEMA(config)["polarity"] == "normal"
        for key, config in merged["circuits"].items():
            assert component.METERING_CIRCUIT_SCHEMA(config)["polarity"] == "inverted"
            unchanged = {k: v for k, v in config.items() if k != "polarity"}
            assert unchanged == topology["circuits"][key]
        merged["circuits"]["cir1"]["polarity"] = "normal"
        assert component.METERING_CIRCUIT_SCHEMA(merged["circuits"]["cir1"])["polarity"] == "normal"
    print("PASS polarity schema: mains/circuits/legacy defaults, explicit values, invalid values and all Vue 3 topologies")


if __name__ == "__main__":
    main()
