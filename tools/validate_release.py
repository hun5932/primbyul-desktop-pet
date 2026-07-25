#!/usr/bin/env python3
"""Validate generated Windows/macOS release artifacts without running them."""

from __future__ import annotations

import hashlib
import os
import plistlib
import stat
import struct
import zipfile
from pathlib import Path

from common import DIST_DIR, load_config, version_parts


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"RELEASE CHECK FAILED: {message}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_pe(path: Path) -> bool:
    data = path.read_bytes()
    require(data[:2] == b"MZ", "Windows output has no MZ header")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[pe_offset:pe_offset + 4] == b"PE\0\0", "invalid PE signature")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    require(machine == 0x8664, f"unexpected PE machine: {machine:#x}")
    optional = pe_offset + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    require(magic == 0x20B, "Windows output is not PE32+")
    subsystem = struct.unpack_from("<H", data, optional + 68)[0]
    require(subsystem == 2, "Windows output is not a GUI subsystem app")
    security_offset, security_size = struct.unpack_from(
        "<II", data, optional + 112 + 8 * 4
    )
    return security_offset != 0 and security_size != 0


def validate_universal(path: Path) -> None:
    data = path.read_bytes()
    magic, count = struct.unpack_from(">II", data, 0)
    require(magic == 0xCAFEBABE and count == 2, "macOS executable is not two-slice Universal")
    cpu_types: set[int] = set()
    for index in range(count):
        cpu_type, _subtype, offset, size, alignment = struct.unpack_from(
            ">IIIII", data, 8 + index * 20
        )
        require(offset + size <= len(data), "Mach-O slice exceeds file bounds")
        require(offset % (1 << alignment) == 0, "Mach-O slice alignment error")
        require(struct.unpack_from("<I", data, offset)[0] == 0xFEEDFACF, "invalid Mach-O slice")
        cpu_types.add(cpu_type)
    require(cpu_types == {0x01000007, 0x0100000C}, "Universal slices are not x86_64 + arm64")


def main() -> None:
    config = load_config()
    version, short = version_parts()
    exe = DIST_DIR / f"Primbyul_v{short}.exe"
    mac_zip = DIST_DIR / f"Primbyul_v{short}_macOS.zip"
    mac_root = DIST_DIR / f"Primbyul_v{short}_macOS"
    app = mac_root / "Primbyul.app"
    mac_executable = app / "Contents" / "MacOS" / "Primbyul"

    require(exe.is_file() and exe.stat().st_size > 1_000_000, "Windows EXE missing or too small")
    require(mac_zip.is_file() and mac_zip.stat().st_size > 1_000_000, "macOS ZIP missing or too small")
    windows_signed = validate_pe(exe)
    validate_universal(mac_executable)
    require(
        os.stat(mac_executable).st_mode & stat.S_IXUSR,
        "macOS executable permission is missing",
    )

    with (app / "Contents" / "Info.plist").open("rb") as handle:
        info = plistlib.load(handle)
    require(info["CFBundleVersion"] == version, "built app version mismatch")
    require(info["CFBundleIdentifier"] == config["bundle_identifier"], "built app bundle ID mismatch")
    frames = list((app / "Contents" / "Resources" / "frames").rglob("*.png"))
    require(len(frames) == 56, f"built app should contain 56 frames, got {len(frames)}")

    with zipfile.ZipFile(mac_zip) as archive:
        require(archive.testzip() is None, "macOS ZIP CRC failure")
        executable_name = (
            f"Primbyul_v{short}_macOS/Primbyul.app/Contents/MacOS/Primbyul"
        )
        executable_info = archive.getinfo(executable_name)
        zipped_mode = executable_info.external_attr >> 16
        require(zipped_mode & stat.S_IXUSR, "ZIP did not preserve macOS executable permission")

    forbidden_markers = (b"powershell", b"cmd.exe", b"curl ", b"wget ")
    exe_lower = exe.read_bytes().lower()
    require(
        not any(marker in exe_lower for marker in forbidden_markers),
        "Windows binary contains an unexpected shell/download marker",
    )
    print(f"Windows: {exe.name} sha256={sha256(exe)} signed={windows_signed}")
    print(f"macOS: {mac_zip.name} sha256={sha256(mac_zip)} universal=x86_64+arm64")
    if not windows_signed:
        print("Notice: Windows output is unsigned; signing is a separate release step.")
    print("Release validation passed.")


if __name__ == "__main__":
    main()
