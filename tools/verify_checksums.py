#!/usr/bin/env python3
"""Verify that an extracted source package still matches its SHA-256 manifest."""

from __future__ import annotations

import hashlib
from pathlib import Path

from common import ROOT


MANIFEST = ROOT / "SOURCE-CHECKSUMS.sha256"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    if not MANIFEST.is_file():
        raise SystemExit(
            "SOURCE-CHECKSUMS.sha256 is missing. "
            "This check is intended for an extracted source ZIP."
        )

    checked = 0
    for number, raw_line in enumerate(
        MANIFEST.read_text(encoding="utf-8").splitlines(),
        start=1,
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            expected, relative = line.split("  ", 1)
        except ValueError as error:
            raise SystemExit(f"Invalid checksum line {number}") from error
        path = ROOT / relative
        if not path.is_file():
            raise SystemExit(f"Missing packaged file: {relative}")
        actual = sha256(path)
        if actual != expected:
            raise SystemExit(f"Checksum mismatch: {relative}")
        checked += 1

    if checked == 0:
        raise SystemExit("Checksum manifest contains no file entries")
    print(f"Source package integrity passed: {checked} files")


if __name__ == "__main__":
    main()
