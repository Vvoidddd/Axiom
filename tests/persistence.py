#!/usr/bin/env python3
"""A real SATA image must retain an account and note across independent boots."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import struct
import zlib


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


harness = load("account_input", Path(__file__).with_name("account-input.py"))
creator = load("disk_creator", Path(__file__).parent.parent / "scripts/create-data-disk.py")
disk_tools = load("disk_tools", Path(__file__).parent.parent / "scripts/data-disk.py")


def first(qmp, hmp, wait_for, key, type_text, click, screenshot, pixel, position, log):
    wait_for("[CHECKPOINT] prepared data disk ready")
    key("f2")
    for word in ("elevate", "axiomtest123!", "device"):
        type_text(word); key("spc")
    key("ret"); type_text("rescan"); key("ret")
    wait_for("[CMD] rescan")
    key("f4"); type_text("persistent"); key("spc"); type_text("note")
    click(85, 125)
    wait_for("[DESKTOP-SAVED] /users/Void/note-1.txt")
    assert log.read_text().count("[CHECKPOINT] saved session committed") >= 2
    screenshot("persistent-note")


def second(qmp, hmp, wait_for, key, type_text, click, screenshot, pixel, position, log):
    wait_for("[CHECKPOINT] restored saved session")
    assert "first administrator account created" not in log.read_text()
    assert "one-time recovery key acknowledged" not in log.read_text()
    key("f3"); key("down"); key("ret")
    screenshot("restored-note")
    click(85, 125)
    wait_for("[DESKTOP-SAVED] /users/Void/note-2.txt")


for mode in ("bios", "uefi"):
    with tempfile.TemporaryDirectory() as tmp:
        disk = Path(tmp) / "data.img"
        creator.create(disk)
        extra = ["-drive", f"if=none,id=data,format=raw,file={disk}", "-device", "ide-hd,drive=data,bus=ide.1"]
        harness.run(mode, first, extra)
        backup, restored = Path(tmp) / "backup.img", Path(tmp) / "restored.img"
        disk_tools.copy_validated(disk, backup)
        disk_tools.copy_validated(backup, restored)
        disk = restored
        extra = ["-drive", f"if=none,id=data,format=raw,file={disk}", "-device", "ide-hd,drive=data,bus=ide.1"]
        harness.run(mode, second, extra, existing=True)
        # Inspect committed bytes independently of the guest renderer.
        slots = []
        with disk.open("rb") as f:
            for lba in (2048, 34816):
                f.seek(lba * 512)
                header = f.read(512)
                magic, generation, version, size, checksum, header_crc = struct.unpack_from("<QQIIII", header)
                assert zlib.crc32(header[:28]) == header_crc
                data = f.read(size)
                assert zlib.crc32(data) == checksum
                slots.append((generation, data))
        data = max(slots)[1]
        assert data.count(b"persistent note") >= 2, "Saved or restored file contents were lost"
        print(f"PASS: {mode} SATA persistence, rescan, backup/restore and account/file round trips across independent boots", flush=True)
