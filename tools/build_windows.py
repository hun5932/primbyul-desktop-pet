#!/usr/bin/env python3
"""Build the single-file Windows GUI executable with Zig."""

from __future__ import annotations

from common import BUILD_DIR, DIST_DIR, ROOT, find_zig, load_config, reset_directory, run, version_parts


def main() -> None:
    config = load_config()
    version, short_version = version_parts()
    zig = find_zig()
    build_dir = BUILD_DIR / "windows"
    build_dir.parent.mkdir(parents=True, exist_ok=True)
    reset_directory(build_dir)
    DIST_DIR.mkdir(parents=True, exist_ok=True)

    source = ROOT / "src" / "windows" / "primbyul.c"
    resources = ROOT / "src" / "windows" / "primbyul.rc"
    compiled_resources = build_dir / "Primbyul.res"
    output = DIST_DIR / f"Primbyul_v{short_version}.exe"

    run([
        zig, "rc", "/c", "65001", "/fo", compiled_resources, "--", resources
    ])
    run([
        zig,
        "cc",
        "-target",
        config["windows_target"],
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-municode",
        source,
        compiled_resources,
        "-o",
        output,
        "-lole32",
        "-lshell32",
        "-lgdiplus",
        "-luser32",
        "-lgdi32",
        "-ladvapi32",
        "-Wl,--subsystem,windows",
    ])
    print(f"Windows build complete: {output} (version {version})")


if __name__ == "__main__":
    main()
