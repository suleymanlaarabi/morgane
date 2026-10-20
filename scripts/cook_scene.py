#!/usr/bin/env python3
"""Wrap a siecs JSON scene in a SIECSSCN v1 header (magic + version + payload size)."""

import struct
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: cook_scene.py scene.json scene.siecsscn", file=sys.stderr)
        return 2
    payload = Path(sys.argv[1]).read_bytes()
    if payload.startswith(b"SIECSSCN"):
        print("source is already a cooked scene", file=sys.stderr)
        return 1
    header = b"SIECSSCN" + struct.pack("<II", 1, len(payload))
    Path(sys.argv[2]).write_bytes(header + payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
