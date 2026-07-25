#!/usr/bin/env python3
"""Package exactly the files tracked by Git into a repository-ready ZIP."""

from __future__ import annotations

import stat
import subprocess
import zipfile
from pathlib import Path

from common import DIST_DIR, ROOT, version_parts


def tracked_paths() -> list[Path]:
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
    _version, short = version_parts()
    DIST_DIR.mkdir(parents=True, exist_ok=True)
    output = DIST_DIR / f"Primbyul_v{short}_GitHub.zip"
    output.unlink(missing_ok=True)
    root_name = "Primbyul"
    paths = tracked_paths()

    with zipfile.ZipFile(
        output,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
        allowZip64=True,
    ) as archive:
        for path in paths:
            relative = path.relative_to(ROOT)
            if not path.is_file():
                continue
            info = zipfile.ZipInfo.from_file(path, f"{root_name}/{relative.as_posix()}")
            if path.suffix in {".py", ".sh"}:
                info.external_attr = (stat.S_IFREG | 0o755) << 16
            archive.writestr(info, path.read_bytes())

    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise SystemExit("GitHub package CRC validation failed")
    print(f"GitHub package complete: {output}")
    print(f"Tracked files included: {len(paths)}")


if __name__ == "__main__":
    main()
