#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import re
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FW_DIR = ROOT / "firmware" / "samd09"
FW_SOURCE = FW_DIR / "main.c"
BUILD_BIN = FW_DIR / "build" / "EmporiaSamd09.bin"
OUT = ROOT / "components" / "emporiavue" / "samd09_firmware.h"

FLASH_SIZE = 16 * 1024
MARKER = b"EMPORIAVUE-SAMD"
FOOTER_FORMAT = "<HHI32s15s9s"
FOOTER_SIZE = struct.calcsize(FOOTER_FORMAT)


def read_numeric_define(name: str) -> int:
    match = re.search(rf"^\s*#define\s+{name}\s+(\d+)\s*$", FW_SOURCE.read_text(), re.MULTILINE)
    if match is None:
        raise SystemExit(f"missing numeric define {name} in {FW_SOURCE}")
    return int(match.group(1))


def bytes_to_c_array(data: bytes, indent: str = "    ") -> str:
    lines: list[str] = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append(indent + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return "\n".join(lines)


def main() -> None:
    hardware_id = read_numeric_define("EMPORIAVUE_HARDWARE_ID")
    mode_id = read_numeric_define("EMPORIAVUE_MODE_ID")
    firmware_version = read_numeric_define("EMPORIAVUE_FIRMWARE_VERSION")

    subprocess.run(["make", "clean"], cwd=FW_DIR, check=True)
    subprocess.run(["make"], cwd=FW_DIR, check=True)

    raw = BUILD_BIN.read_bytes()
    if len(raw) > FLASH_SIZE - FOOTER_SIZE:
        raise SystemExit(f"firmware is too large: {len(raw)} bytes, max {FLASH_SIZE - FOOTER_SIZE}")

    payload = raw + (b"\xff" * (FLASH_SIZE - FOOTER_SIZE - len(raw)))
    payload_sha = hashlib.sha256(payload).digest()
    footer = struct.pack(
        FOOTER_FORMAT,
        hardware_id,
        mode_id,
        firmware_version,
        payload_sha,
        MARKER,
        b"\xff" * 9,
    )
    image = payload + footer
    image_sha = hashlib.sha256(image).digest()

    header = f"""#pragma once

#include <cstdint>

namespace esphome {{
namespace emporiavue {{

static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_HARDWARE_ID = {hardware_id}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_MODE_ID = {mode_id}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_VERSION = {firmware_version}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_SIZE = {len(image)}UL;
static constexpr uint32_t BUNDLED_SAMD_FIRMWARE_SOURCE_SIZE = {len(raw)}UL;
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE_SHA256[32] = {{
{bytes_to_c_array(image_sha)}
}};
static constexpr uint8_t BUNDLED_SAMD_FIRMWARE_PAYLOAD_SHA256[32] = {{
{bytes_to_c_array(payload_sha)}
}};

static constexpr uint8_t BUNDLED_SAMD_FIRMWARE[BUNDLED_SAMD_FIRMWARE_SIZE] = {{
{bytes_to_c_array(image)}
}};

}}  // namespace emporiavue
}}  // namespace esphome
"""
    OUT.write_text(header)
    print(f"wrote {OUT}")
    print(
        f"hardware_id={hardware_id} mode_id={mode_id} firmware_version={firmware_version} "
        f"display=v{firmware_version // 10}.{firmware_version % 10}"
    )
    print(f"source_size={len(raw)} image_size={len(image)}")
    print(f"image_sha256={image_sha.hex()}")
    print(f"payload_sha256={payload_sha.hex()}")


if __name__ == "__main__":
    main()
