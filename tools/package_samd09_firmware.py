#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import argparse
import re
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FW_DIR = ROOT / "firmware" / "samd09"
BUILD_BIN = FW_DIR / "build" / "EmporiaSamd09.bin"

FLASH_SIZE = 16 * 1024
MARKER = b"EMPORIAVUE-SAMD"
FOOTER_FORMAT = "<HHI32s15s9s"
FOOTER_SIZE = struct.calcsize(FOOTER_FORMAT)
HARDWARE_SLUGS = {
    2: "vue2",
}
MODE_SLUGS = {
    1: "i2c",
    2: "spi",
}


def read_numeric_define(source: Path, name: str) -> int:
    match = re.search(rf"^\s*#define\s+{name}\s+(\d+)\s*$", source.read_text(), re.MULTILINE)
    if match is None:
        raise SystemExit(f"missing numeric define {name} in {source}")
    return int(match.group(1))


def bytes_to_c_array(data: bytes, indent: str = "    ") -> str:
    lines: list[str] = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append(indent + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return "\n".join(lines)


def display_version(version: int) -> str:
    return f"v{version // 10}.{version % 10}"


def image_output_path(hardware_id: int, mode_id: int, firmware_version: int) -> Path:
    hardware_slug = HARDWARE_SLUGS.get(hardware_id, f"hw{hardware_id}")
    mode_slug = MODE_SLUGS.get(mode_id, f"mode{mode_id}")
    return FW_DIR / "images" / mode_slug / f"{hardware_slug}-{mode_slug}-{display_version(firmware_version)}.bin"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="main.c", help="firmware source file under firmware/samd09")
    parser.add_argument(
        "--out",
        default=str(ROOT / "components" / "emporiavue" / "samd09_firmware.h"),
        help="generated C++ header path",
    )
    parser.add_argument("--symbol-prefix", default="BUNDLED_SAMD_FIRMWARE", help="C++ symbol prefix")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    firmware_source = FW_DIR / args.source
    out = Path(args.out)
    symbol_prefix = args.symbol_prefix

    hardware_id = read_numeric_define(firmware_source, "EMPORIAVUE_HARDWARE_ID")
    mode_id = read_numeric_define(firmware_source, "EMPORIAVUE_MODE_ID")
    firmware_version = read_numeric_define(firmware_source, "EMPORIAVUE_FIRMWARE_VERSION")

    subprocess.run(["make", "clean"], cwd=FW_DIR, check=True)
    subprocess.run(["make", f"SOURCE={args.source}"], cwd=FW_DIR, check=True)

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
    image_out = image_output_path(hardware_id, mode_id, firmware_version)

    header = f"""#pragma once

#include <cstdint>

namespace esphome {{
namespace emporiavue {{

static constexpr uint32_t {symbol_prefix}_HARDWARE_ID = {hardware_id}UL;
static constexpr uint32_t {symbol_prefix}_MODE_ID = {mode_id}UL;
static constexpr uint32_t {symbol_prefix}_VERSION = {firmware_version}UL;
static constexpr uint32_t {symbol_prefix}_SIZE = {len(image)}UL;
static constexpr uint32_t {symbol_prefix}_SOURCE_SIZE = {len(raw)}UL;
static constexpr uint8_t {symbol_prefix}_SHA256[32] = {{
{bytes_to_c_array(image_sha)}
}};
static constexpr uint8_t {symbol_prefix}_PAYLOAD_SHA256[32] = {{
{bytes_to_c_array(payload_sha)}
}};

static constexpr uint8_t {symbol_prefix}[{symbol_prefix}_SIZE] = {{
{bytes_to_c_array(image)}
}};

}}  // namespace emporiavue
}}  // namespace esphome
"""
    out.write_text(header)
    image_out.parent.mkdir(parents=True, exist_ok=True)
    image_out.write_bytes(image)
    print(f"wrote {out}")
    print(f"wrote {image_out}")
    print(
        f"hardware_id={hardware_id} mode_id={mode_id} firmware_version={firmware_version} "
        f"display={display_version(firmware_version)}"
    )
    print(f"source_size={len(raw)} image_size={len(image)}")
    print(f"image_sha256={image_sha.hex()}")
    print(f"payload_sha256={payload_sha.hex()}")


if __name__ == "__main__":
    main()
