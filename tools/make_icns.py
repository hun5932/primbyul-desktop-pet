#!/usr/bin/env python3
"""Create a modern PNG-backed ICNS file from the v1.5 master icon."""

from __future__ import annotations

import argparse
import struct
from io import BytesIO
from pathlib import Path

from PIL import Image


ENTRIES = (("ic07", 128), ("ic08", 256), ("ic09", 512), ("ic10", 1024))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = Image.open(args.source).convert("RGBA")
    chunks: list[bytes] = []
    for kind, size in ENTRIES:
        image = source.resize((size, size), Image.Resampling.LANCZOS)
        encoded = BytesIO()
        image.save(encoded, format="PNG", compress_level=6)
        data = encoded.getvalue()
        chunks.append(kind.encode("ascii") + struct.pack(">I", len(data) + 8) + data)
    body = b"".join(chunks)
    args.output.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)


if __name__ == "__main__":
    main()
