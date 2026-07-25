#!/usr/bin/env python3
"""Update every source-of-truth version field from one semantic version."""

from __future__ import annotations

import argparse
import json
import plistlib
import re

from common import CONFIG_PATH, ROOT, load_config


def replace_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f"Could not update {label}; expected one match, got {count}")
    return updated


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("version", help="semantic version, for example 1.6.0")
    args = parser.parse_args()
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", args.version)
    if not match:
        raise SystemExit("Version must use MAJOR.MINOR.PATCH, for example 1.6.0")
    major, minor, patch = match.groups()
    short = f"{major}.{minor}"
    csv = f"{major},{minor},{patch},0"

    config = load_config()
    config["version"] = args.version
    CONFIG_PATH.write_text(
        json.dumps(config, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    rc_path = ROOT / "src" / "windows" / "primbyul.rc"
    rc = rc_path.read_text(encoding="utf-8")
    rc = replace_once(rc, r"^ FILEVERSION .+$", f" FILEVERSION {csv}", "FILEVERSION")
    rc = replace_once(rc, r"^ PRODUCTVERSION .+$", f" PRODUCTVERSION {csv}", "PRODUCTVERSION")
    rc = replace_once(
        rc,
        r'VALUE "FileVersion", "[^"]+\\0"',
        f'VALUE "FileVersion", "{args.version}\\\\0"',
        "FileVersion string",
    )
    rc = replace_once(
        rc,
        r'VALUE "OriginalFilename", "[^"]+\\0"',
        f'VALUE "OriginalFilename", "Primbyul_v{short}.exe\\\\0"',
        "OriginalFilename",
    )
    rc = replace_once(
        rc,
        r'VALUE "ProductVersion", "[^"]+\\0"',
        f'VALUE "ProductVersion", "{args.version}\\\\0"',
        "ProductVersion string",
    )
    rc_path.write_text(rc, encoding="utf-8")

    manifest_path = ROOT / "src" / "windows" / "Primbyul.exe.manifest"
    manifest = manifest_path.read_text(encoding="utf-8")
    manifest = replace_once(
        manifest,
        r'version="\d+\.\d+\.\d+\.0"',
        f'version="{args.version}.0"',
        "manifest version",
    )
    manifest_path.write_text(manifest, encoding="utf-8")

    plist_path = ROOT / "src" / "macos" / "Info.plist"
    with plist_path.open("rb") as handle:
        info = plistlib.load(handle)
    info["CFBundleShortVersionString"] = short
    info["CFBundleVersion"] = args.version
    with plist_path.open("wb") as handle:
        plistlib.dump(info, handle, sort_keys=False)

    mac_path = ROOT / "src" / "macos" / "primbyul_mac.c"
    mac = mac_path.read_text(encoding="utf-8")
    mac = replace_once(
        mac,
        r'add_item\(g_menu, "프림별 v\d+\.\d+", NULL\);',
        f'add_item(g_menu, "프림별 v{short}", NULL);',
        "macOS menu version",
    )
    mac_path.write_text(mac, encoding="utf-8")
    print(f"Updated project version to {args.version}.")
    print("Next: add CHANGELOG notes, then run python tools/build_all.py")


if __name__ == "__main__":
    main()
