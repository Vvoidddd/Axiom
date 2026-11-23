#!/usr/bin/env python3
"""Exercise real page tables and injected allocation failures on BIOS/UEFI."""
import importlib.util
from pathlib import Path
import time

spec = importlib.util.spec_from_file_location("account_input", Path(__file__).with_name("account-input.py"))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)

def exercise(qmp, hmp, wait_for, key, type_text, click, screenshot, pixel, position, log):
    wait_for("[DESKTOP] session ready")
    key("f2")
    for attempt in range(3):
        type_text("memfailtest"); key("ret")
        deadline = time.monotonic() + 10
        while log.read_text().count("[PASS] MEMORY ALLOCATION ROLLBACK") < attempt + 1:
            text = log.read_text()
            assert "[FAIL] MEMORY ALLOCATION ROLLBACK" not in text, text[-2000:]
            assert "CPU exception" not in text, text[-2000:]
            assert time.monotonic() < deadline, text[-2000:]
            time.sleep(.05)
    type_text("heaptest"); key("ret")
    wait_for("[PASS] HEAP MAP ALLOCATE FREE TEST")
    screenshot("memory-rollback")
    key("f7")
    wait_for("[DESKTOP] Task Manager")
    screenshot("memory-tasks")
    assert "CPU exception" not in log.read_text()
    print("PASS: allocation rollback, guarded stacks, MMIO, user W^X and private page-table destruction", flush=True)

for mode in ("bios", "uefi"):
    harness.run(mode, exercise)
