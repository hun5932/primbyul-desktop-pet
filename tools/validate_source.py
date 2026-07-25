#!/usr/bin/env python3
"""Check source/version/resource consistency before compiling."""

from __future__ import annotations

import plistlib
import re
import struct
from pathlib import Path

from common import ROOT, load_config


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"invalid PNG: {path}")
    return struct.unpack(">II", data[16:24])


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"SOURCE CHECK FAILED: {message}")


def main() -> None:
    config = load_config()
    version = str(config["version"])
    major, minor, patch = version.split(".")
    version_csv = f"{major},{minor},{patch},0"
    short = f"{major}.{minor}"

    windows = ROOT / "src" / "windows"
    macos = ROOT / "src" / "macos"
    rc_path = windows / "primbyul.rc"
    rc = rc_path.read_text(encoding="utf-8")
    manifest = (windows / "Primbyul.exe.manifest").read_text(encoding="utf-8")
    mac_source = (macos / "primbyul_mac.c").read_text(encoding="utf-8")
    with (macos / "Info.plist").open("rb") as handle:
        info = plistlib.load(handle)

    require(f"FILEVERSION {version_csv}" in rc, "Windows FILEVERSION mismatch")
    require(f"PRODUCTVERSION {version_csv}" in rc, "Windows PRODUCTVERSION mismatch")
    require(f'VALUE "FileVersion", "{version}\\0"' in rc, "Windows string version mismatch")
    require(
        f'VALUE "OriginalFilename", "Primbyul_v{short}.exe\\0"' in rc,
        "Windows output filename mismatch",
    )
    require(f'version="{version}.0"' in manifest, "manifest version mismatch")
    require(info["CFBundleShortVersionString"] == short, "macOS short version mismatch")
    require(info["CFBundleVersion"] == version, "macOS build version mismatch")
    require(info["CFBundleIdentifier"] == config["bundle_identifier"], "bundle identifier mismatch")
    require(f"프림별 v{short}" in mac_source, "macOS menu version mismatch")

    resource_rows = re.findall(
        r"^(\d+)\s+RCDATA\s+\"([^\"]+)\"",
        rc,
        flags=re.MULTILINE,
    )
    require(len(resource_rows) == 48, f"expected 48 Windows frame resources, got {len(resource_rows)}")
    resource_ids = [int(identifier) for identifier, _path in resource_rows]
    require(len(resource_ids) == len(set(resource_ids)), "duplicate Windows resource ID")
    for _identifier, relative in resource_rows:
        path = (rc_path.parent / relative).resolve()
        require(path.is_file(), f"missing resource: {path}")
        require(png_size(path) == (512, 512), f"wrong frame size: {path}")

    frames = sorted((ROOT / "assets" / "frames").rglob("*.png"))
    keyframes = sorted((ROOT / "assets" / "source-keyframes").rglob("key-*.png"))
    references = sorted((ROOT / "assets" / "reference-photos").glob("*.png"))
    require(len(frames) == 56, f"expected 56 release frames, got {len(frames)}")
    require(len(keyframes) == 48, f"expected 48 source keyframes, got {len(keyframes)}")
    # Reference photos help with future visual redesigns but are deliberately
    # excluded from the GitHub repository for privacy. A local production
    # workspace may still keep the complete seven-photo set.
    require(
        len(references) == 0 or len(references) >= 7,
        "reference photos must be absent or contain the complete seven-photo set",
    )
    for path in frames:
        require(png_size(path) == (512, 512), f"wrong frame size: {path}")

    try:
        from PIL import Image
    except ImportError:
        print("Pillow not installed: skipped deep alpha-edge validation.")
    else:
        for path in frames:
            image = Image.open(path).convert("RGBA")
            alpha = image.getchannel("A")
            edges = (
                alpha.crop((0, 0, 512, 1)),
                alpha.crop((0, 511, 512, 512)),
                alpha.crop((0, 0, 1, 512)),
                alpha.crop((511, 0, 512, 512)),
            )
            require(
                all(edge.getbbox() is None for edge in edges),
                f"frame touches cell edge: {path}",
            )

    forbidden = list((ROOT / "src").rglob("*.exe")) + list((ROOT / "src").rglob("*.app"))
    require(not forbidden, "compiled binaries must not be stored under src/")
    print(
        "Source validation passed:",
        f"version={version}",
        f"release_frames={len(frames)}",
        f"source_keyframes={len(keyframes)}",
        f"local_reference_photos={len(references)}",
        f"windows_resources={len(resource_rows)}",
    )


if __name__ == "__main__":
    main()
