#!/usr/bin/env python3
"""Safe boot runs an authenticated ring-3 shell with services disabled across login."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import time

spec = importlib.util.spec_from_file_location("account_input", Path(__file__).with_name("account-input.py"))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)

def exercise(qmp, hmp, wait_for, key, type_text, click, screenshot, pixel, position, log):
    def command(text):
        for c in text:
            key({" ": "spc", "-": "minus", "!": "shift-1"}.get(c, c))
        key("ret")
    wait_for("safe mode: background services disabled until reboot")
    command("echo safe-shell-alive");wait_for("[CMD] echo safe-shell-alive")
    command("elevate service axiomtest123!")
    command("service start system-logger --confirm");wait_for("safe mode refused service start")
    screenshot("safe-shell")
    ready = log.read_text().count("[FORM] login ready")
    command("logout")
    end = time.monotonic() + 10
    while log.read_text().count("[FORM] login ready") <= ready:
        assert time.monotonic() < end
        time.sleep(.05)
    position[:] = [512, 384]
    click(280,250);type_text("Void");click(280,350);type_text("axiomtest123!");click(500,450)
    end = time.monotonic() + 10
    while log.read_text().count("launching user-space init and shell") < 2:
        assert time.monotonic() < end
        time.sleep(.05)
    command("echo safe-relogin-alive");wait_for("[CMD] echo safe-relogin-alive")
    assert "[SERVICE-SPAWN]" not in log.read_text()
    assert "[DESKTOP] session ready" not in log.read_text()
    assert "[SPAWN] /apps/hello" not in log.read_text()
    screenshot("safe-relogin")
    print("PASS: safe ring-3 shell, service-start denial, no desktop/autostart and repeat login", flush=True)

with tempfile.TemporaryDirectory() as tmp:
    tree = Path(tmp) / "tree"
    with (harness.out / "safe-image.log").open("w") as build_log:
        subprocess.run(["xorriso", "-osirrox", "on", "-indev", str(harness.iso), "-extract", "/", str(tree)], check=True, stdout=build_log, stderr=build_log)
        config = tree / "boot/limine/limine.conf";config.chmod(0o600)
        config.write_text("timeout: 0\n/Axiom safe mode\n    protocol: limine\n    path: boot():/boot/axiom.elf\n    cmdline: safe\n    resolution: 1024x768x32\n")
        image = Path(tmp) / "safe.iso"
        subprocess.run(["xorriso", "-as", "mkisofs", "-R", "-r", "-J", "-b", "boot/limine/limine-bios-cd.bin",
                        "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table", "--efi-boot", "boot/limine/limine-uefi-cd.bin",
                        "-efi-boot-part", "--efi-boot-image", str(tree), "-o", str(image)], check=True, stdout=build_log, stderr=build_log)
    harness.iso = image
    for firmware in ("bios", "uefi"):
        harness.run(firmware, exercise)
