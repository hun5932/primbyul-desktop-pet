#!/usr/bin/env python3
"""Pack x86_64 and arm64 Mach-O executables into one Universal binary."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


FAT_MAGIC = 0xCAFEBABE
CPU_TYPE_X86_64 = 0x01000007
CPU_TYPE_ARM64 = 0x0100000C
CPU_SUBTYPE_X86_64_ALL = 3
CPU_SUBTYPE_ARM64_ALL = 0
ALIGN_POWER = 14
ALIGNMENT = 1 << ALIGN_POWER


def aligned(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x86_64", type=Path)
    parser.add_argument("arm64", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    x86 = args.x86_64.read_bytes()
    arm = args.arm64.read_bytes()
    header_size = 8 + 20 * 2
    x86_offset = aligned(header_size)
    arm_offset = aligned(x86_offset + len(x86))
    total = arm_offset + len(arm)
    payload = bytearray(total)
    struct.pack_into(">II", payload, 0, FAT_MAGIC, 2)
    struct.pack_into(
        ">IIIII",
        payload,
        8,
        CPU_TYPE_X86_64,
        CPU_SUBTYPE_X86_64_ALL,
        x86_offset,
        len(x86),
        ALIGN_POWER,
    )
    struct.pack_into(
        ">IIIII",
        payload,
        28,
        CPU_TYPE_ARM64,
        CPU_SUBTYPE_ARM64_ALL,
        arm_offset,
        len(arm),
        ALIGN_POWER,
    )
    payload[x86_offset:x86_offset + len(x86)] = x86
    payload[arm_offset:arm_offset + len(arm)] = arm
    args.output.write_bytes(payload)
    args.output.chmod(0o755)


if __name__ == "__main__":
    main()
