#!/usr/bin/env python3
"""Inspect, back up, or restore raw Axiom data images while their VM is off."""
import argparse
from pathlib import Path
import shutil
import struct
import zlib

SIZE = 64 * 1024 * 1024


def inspect(path):
    with Path(path).open("rb") as image:
        if image.seek(0, 2) != SIZE:
            raise ValueError("Expected a 64 MiB raw Axiom data image")
        image.seek(0)
        label = image.read(512)
        fields = struct.unpack_from("<QIIIII", label)
        if fields != (0x3130415441445841, 1, 131072, 2048, 34816, zlib.crc32(label[:24])):
            raise ValueError("Invalid Axiom data label")
        generations, empty = [], 0
        for lba in (2048, 34816):
            image.seek(lba * 512)
            header = image.read(512)
            if header == bytes(512):
                empty += 1
                continue
            magic, generation, version, size, data_crc, header_crc = struct.unpack_from("<QQIIII", header)
            if (magic != 0x31544f4c53565841 or version != 1 or not 0 < generation < 2**64 - 1
                    or not 144 <= size <= 8 * 1024 * 1024 or header_crc != zlib.crc32(header[:28])):
                continue
            data = image.read(size)
            if len(data) != size or zlib.crc32(data) != data_crc:
                continue
            magic, version, count = struct.unpack_from("<QII", data)
            if magic == 0x3150414e53565841 and version == 1 and 2 <= count < 256 and 144 + count * 392 <= size:
                generations.append(generation)
        if not generations and empty != 2:
            raise ValueError("No valid saved generation; preserve the original for recovery")
        return sorted(generations)


def copy_validated(source, destination):
    source, destination = Path(source), Path(destination)
    inspect(source)
    before = source.stat()
    # Exclusive creation prevents an incorrect path from overwriting either image.
    with destination.open("xb") as output:
        try:
            with source.open("rb") as input_file:
                shutil.copyfileobj(input_file, output, 1024 * 1024)
            output.flush()
            import os
            os.fsync(output.fileno())
        except BaseException:
            output.close()
            destination.unlink()
            raise
    try:
        after = source.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError("Source changed during copy; power off its VM first")
        return inspect(destination)
    except BaseException:
        destination.unlink()
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("inspect", "backup", "restore"))
    parser.add_argument("source")
    parser.add_argument("destination", nargs="?")
    args = parser.parse_args()
    if args.action != "inspect" and not args.destination:
        parser.error("backup/restore requires a NEW destination path")
    generations = inspect(args.source) if args.action == "inspect" else copy_validated(args.source, args.destination)
    print("Valid generations:", generations or "blank prepared image")
