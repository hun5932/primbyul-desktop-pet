#!/usr/bin/env python3
"""Validate sources, build both platforms, then validate the release output."""

from __future__ import annotations

import sys

from common import ROOT, run


def main() -> None:
    run([sys.executable, ROOT / "tools" / "validate_source.py"])
    run([sys.executable, ROOT / "tools" / "build_windows.py"])
    run([sys.executable, ROOT / "tools" / "build_macos.py"])
    run([sys.executable, ROOT / "tools" / "validate_release.py"])


if __name__ == "__main__":
    main()
