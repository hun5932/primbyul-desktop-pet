#!/usr/bin/env python3
"""Write SHA-256 hashes for the current version's distributable files."""

from __future__ import annotations

import hashlib
from pathlib import Path

from common import DIST_DIR, version_parts


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    _version, short = version_parts()
    expected = [
        DIST_DIR / f"Primbyul_v{short}.exe",
        DIST_DIR / f"Primbyul_v{short}_macOS.zip",
        DIST_DIR / f"Primbyul_v{short}_Source.zip",
    ]
    missing = [path.name for path in expected if not path.is_file()]
    if missing:
        raise SystemExit(f"Cannot write release checksums; missing: {missing}")

    output = DIST_DIR / "SHA256SUMS.txt"
    output.write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in expected),
        encoding="utf-8",
    )
    print(f"Release checksums complete: {output}")


if __name__ == "__main__":
    main()
