#!/usr/bin/env python3
"""Require a release tag such as v1.5.0 to match config/project.json."""

from __future__ import annotations

import argparse
import re

from common import load_config


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("tag", help="Git tag, for example v1.5.0")
    args = parser.parse_args()

    if not re.fullmatch(r"v\d+\.\d+\.\d+", args.tag):
        raise SystemExit("RELEASE TAG CHECK FAILED: tag must look like v1.5.0")
    expected = f"v{load_config()['version']}"
    if args.tag != expected:
        raise SystemExit(
            f"RELEASE TAG CHECK FAILED: tag {args.tag} does not match {expected}"
        )
    print(f"Release tag validation passed: {args.tag}")


if __name__ == "__main__":
    main()
