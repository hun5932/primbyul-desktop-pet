#!/usr/bin/env python3
"""Shared paths and subprocess helpers for Primbyul build tools."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / "config" / "project.json"
BUILD_DIR = ROOT / "build"
DIST_DIR = ROOT / "dist"


def load_config() -> dict[str, object]:
    return json.loads(CONFIG_PATH.read_text(encoding="utf-8"))


def version_parts() -> tuple[str, str]:
    version = str(load_config()["version"])
    major, minor, _patch = version.split(".")
    return version, f"{major}.{minor}"


def find_zig() -> str:
    explicit = os.environ.get("ZIG")
    if explicit:
        candidate = Path(explicit).expanduser()
        if candidate.is_file():
            return str(candidate.resolve())
        raise SystemExit(f"ZIG environment variable is not a file: {candidate}")
    discovered = shutil.which("zig")
    if discovered:
        return discovered
    raise SystemExit(
        "Zig was not found. Install Zig, add it to PATH, or set the ZIG "
        "environment variable to the zig/zig.exe path."
    )


def reset_directory(path: Path) -> None:
    """Reset only a verified child of this project's build/dist directories."""
    resolved = path.resolve()
    allowed_parents = {BUILD_DIR.resolve(), DIST_DIR.resolve()}
    if resolved.parent not in allowed_parents:
        raise RuntimeError(f"refusing to reset unexpected directory: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    resolved.mkdir(parents=True, exist_ok=True)


def run(command: Iterable[object], *, cwd: Path = ROOT) -> None:
    args = [str(value) for value in command]
    print("+", " ".join(args))
    subprocess.run(args, cwd=cwd, check=True)
