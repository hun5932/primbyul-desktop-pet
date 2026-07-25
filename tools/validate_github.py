#!/usr/bin/env python3
"""Validate GitHub tracking, privacy, workflow, and file-size rules."""

from __future__ import annotations

import subprocess
from pathlib import Path

from common import ROOT


MAX_GITHUB_FILE_SIZE = 100 * 1024 * 1024
SETUP_ZIG_COMMIT = "d1434d08867e3ee9daa34448df10607b98908d29"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"GITHUB CHECK FAILED: {message}")


def tracked_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [
        ROOT / name.decode("utf-8")
        for name in result.stdout.split(b"\0")
        if name
    ]


def main() -> None:
    required = {
        ".github/workflows/ci.yml",
        ".github/workflows/release.yml",
        ".github/dependabot.yml",
        ".gitignore",
        ".gitattributes",
        ".editorconfig",
        "README.md",
        "CONTRIBUTING.md",
        "SECURITY.md",
        "docs/GITHUB.md",
        "tools/publish_github.ps1",
        "tools/publish_github.sh",
    }
    paths = tracked_files()
    relative_names = {path.relative_to(ROOT).as_posix() for path in paths}
    missing = sorted(required - relative_names)
    require(not missing, f"required repository files are missing: {missing}")

    forbidden_secret_suffixes = {".p12", ".pfx", ".key", ".pem"}
    total_size = 0
    for path in paths:
        relative = path.relative_to(ROOT)
        name = relative.as_posix()
        require(path.is_file(), f"tracked path is not a file: {name}")
        size = path.stat().st_size
        total_size += size
        require(
            size < MAX_GITHUB_FILE_SIZE,
            f"file exceeds GitHub's 100 MiB limit: {name}",
        )
        require(
            not (name.startswith("build/") or name.startswith("tools/__pycache__/")),
            f"generated build/cache file is tracked: {name}",
        )
        require(
            not name.startswith("dist/") or name == "dist/.gitkeep",
            f"release output is tracked: {name}",
        )
        require(
            not (
                name.startswith("assets/reference-photos/")
                and path.suffix.lower() in {".png", ".jpg", ".jpeg", ".webp"}
            ),
            f"private reference photo is tracked: {name}",
        )
        require(
            path.suffix.lower() not in forbidden_secret_suffixes,
            f"possible signing secret is tracked: {name}",
        )
        require(
            not (path.name == ".env" or path.name.startswith(".env.")),
            f"environment secret file is tracked: {name}",
        )

    ci = (ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
    release = (ROOT / ".github" / "workflows" / "release.yml").read_text(
        encoding="utf-8"
    )
    workflows = ci + "\n" + release
    require("pull_request_target:" not in workflows, "unsafe pull_request_target is enabled")
    require(
        f"mlugg/setup-zig@{SETUP_ZIG_COMMIT}" in ci
        and f"mlugg/setup-zig@{SETUP_ZIG_COMMIT}" in release,
        "setup-zig must stay pinned to its reviewed commit",
    )
    require("permissions:\n  contents: read" in ci, "CI permissions are not read-only")
    require(
        "permissions:\n  contents: write" in release,
        "release workflow cannot create a release",
    )
    require("--draft" in release, "automated releases must be created as drafts")

    secret_markers = (
        "BEGIN " + "PRIVATE KEY",
        "github_" + "pat_",
        "gh" + "p_",
    )
    for path in paths:
        if path.suffix.lower() not in {
            ".c",
            ".h",
            ".py",
            ".md",
            ".yml",
            ".yaml",
            ".json",
            ".plist",
            ".rc",
            ".manifest",
            ".ps1",
            ".sh",
        }:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        require(
            not any(marker in text for marker in secret_markers),
            f"possible credential marker in tracked file: {path.relative_to(ROOT)}",
        )

    print(
        "GitHub validation passed:",
        f"tracked_files={len(paths)}",
        f"tracked_bytes={total_size}",
        "private_reference_photos=0",
    )


if __name__ == "__main__":
    main()
