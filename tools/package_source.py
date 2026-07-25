#!/usr/bin/env python3
"""Create a clean source archive, excluding generated build/release output."""

from __future__ import annotations

import hashlib
import stat
import zipfile
from pathlib import Path

from common import DIST_DIR, ROOT, version_parts


EXCLUDED_PARTS = {"build", "dist", "__pycache__", ".git"}
EXCLUDED_SUFFIXES = {".pyc", ".pdb", ".res", ".exe"}
EXCLUDED_FILES = {
    # Kept only as a compatibility input in the working tree.  New builds use
    # the version-neutral Primbyul.ico so a version bump never needs a rename.
    Path("assets/icons/Primbyul-v1.5.ico"),
}


def include(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    return (
        not any(part in EXCLUDED_PARTS for part in relative.parts)
        and path.suffix.lower() not in EXCLUDED_SUFFIXES
        and relative not in EXCLUDED_FILES
    )


def main() -> None:
    _version, short = version_parts()
    DIST_DIR.mkdir(parents=True, exist_ok=True)
    output = DIST_DIR / f"Primbyul_v{short}_Source.zip"
    if output.exists():
        output.unlink()
    archive_root = f"Primbyul_v{short}_Source"
    paths = [
        path
        for path in sorted(ROOT.rglob("*"))
        if include(path) and path.is_file()
    ]
    checksum_lines: list[str] = []
    with zipfile.ZipFile(
        output,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
        allowZip64=True,
    ) as archive:
        for path in paths:
            relative = path.relative_to(ROOT).as_posix()
            data = path.read_bytes()
            info = zipfile.ZipInfo.from_file(path, f"{archive_root}/{relative}")
            if path.suffix == ".py":
                info.external_attr = (stat.S_IFREG | 0o755) << 16
            archive.writestr(info, data)
            checksum_lines.append(
                f"{hashlib.sha256(data).hexdigest()}  {relative}"
            )
        checksum_text = (
            "# SHA-256 checksums for every source archive entry except this file.\n"
            + "\n".join(checksum_lines)
            + "\n"
        )
        archive.writestr(
            f"{archive_root}/SOURCE-CHECKSUMS.sha256",
            checksum_text.encode("utf-8"),
        )
    print(f"Source package complete: {output}")
    print(f"Included files: {len(paths)} + SOURCE-CHECKSUMS.sha256")


if __name__ == "__main__":
    main()
