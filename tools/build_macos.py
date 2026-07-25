#!/usr/bin/env python3
"""Build a Universal (x86_64 + arm64) macOS app and ZIP with Zig."""

from __future__ import annotations

import os
import shutil
import stat
import sys
import zipfile
from pathlib import Path

from common import BUILD_DIR, DIST_DIR, ROOT, find_zig, load_config, reset_directory, run, version_parts


def add_tree_to_zip(archive: zipfile.ZipFile, root: Path, base: Path) -> None:
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(base).as_posix()
        if path.is_dir():
            info = zipfile.ZipInfo(relative.rstrip("/") + "/")
            info.external_attr = (stat.S_IFDIR | 0o755) << 16
            archive.writestr(info, b"")
            continue
        info = zipfile.ZipInfo.from_file(path, arcname=relative)
        archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)


def main() -> None:
    config = load_config()
    version, short_version = version_parts()
    zig = find_zig()
    minimum = str(config["macos_minimum"])

    build_dir = BUILD_DIR / "macos"
    package_dir = DIST_DIR / f"Primbyul_v{short_version}_macOS"
    build_dir.parent.mkdir(parents=True, exist_ok=True)
    DIST_DIR.mkdir(parents=True, exist_ok=True)
    reset_directory(build_dir)
    reset_directory(package_dir)

    source = ROOT / "src" / "macos" / "primbyul_mac.c"
    arm64 = build_dir / "Primbyul-arm64"
    x86_64 = build_dir / "Primbyul-x86_64"
    app = package_dir / "Primbyul.app"
    contents = app / "Contents"
    executable = contents / "MacOS" / "Primbyul"
    resources = contents / "Resources"
    executable.parent.mkdir(parents=True)
    resources.mkdir(parents=True)

    common_flags = [
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
        f"-mmacosx-version-min={minimum}",
    ]
    run([
        zig, "cc", "-target", "aarch64-macos-none",
        *common_flags, source, "-o", arm64,
    ])
    run([
        zig, "cc", "-target", "x86_64-macos-none",
        *common_flags, source, "-o", x86_64,
    ])
    run([
        sys.executable,
        ROOT / "tools" / "make_universal.py",
        x86_64,
        arm64,
        executable,
    ])
    run([
        sys.executable,
        ROOT / "tools" / "make_icns.py",
        ROOT / "assets" / "icons" / "primbyul-icon-1024.png",
        resources / "Primbyul.icns",
    ])

    shutil.copy2(ROOT / "src" / "macos" / "Info.plist", contents / "Info.plist")
    shutil.copy2(
        ROOT / "assets" / "icons" / "primbyul-status-icon.png",
        resources / "primbyul-status-icon.png",
    )
    shutil.copytree(
        ROOT / "assets" / "frames",
        resources / "frames",
        dirs_exist_ok=True,
    )
    shutil.copy2(
        ROOT / "docs" / "RUN-MACOS.md",
        package_dir / "README-macOS.md",
    )
    os.chmod(executable, 0o755)

    zip_path = DIST_DIR / f"Primbyul_v{short_version}_macOS.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", allowZip64=True) as archive:
        add_tree_to_zip(archive, package_dir, DIST_DIR)
    print(f"macOS build complete: {zip_path} (version {version})")


if __name__ == "__main__":
    main()
