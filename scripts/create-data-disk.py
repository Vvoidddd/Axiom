#!/usr/bin/env python3
"""Create a NEW 64 MiB Axiom data image. Never opens an existing target for write."""
import argparse
from pathlib import Path
import struct
import zlib


def create(path):
    label = struct.pack("<QIIII", 0x3130415441445841, 1, 131072, 2048, 34816)
    label += struct.pack("<I", zlib.crc32(label))
    with Path(path).open("xb") as image:
        image.write(label.ljust(512, b"\0"))
        image.truncate(64 * 1024 * 1024)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", help="New .img path; existing files are refused")
    args = parser.parse_args()
    create(args.path)
    print(f"Created {args.path}. Attach as a SATA data disk; continue booting the Axiom ISO.")
