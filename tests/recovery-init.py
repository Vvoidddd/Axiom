#!/usr/bin/env python3
"""Corrupt only the bundled init ELF and verify authenticated fallback after login."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile

spec = importlib.util.spec_from_file_location("account_input", Path(__file__).with_name("account-input.py"))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)

def exercise(qmp, hmp, wait_for, key, type_text, click, screenshot, pixel, position, log):
    wait_for("user-space init failed; entering authenticated recovery")
    wait_for("Login: ")
    assert "Authenticated maintenance console." not in log.read_text()
    type_text("Void");key("ret");type_text("wrongpass");key("ret")
    wait_for("Recovery login rejected.")
    type_text("Void");key("ret");type_text("axiomtest123!");key("ret")
    wait_for("Authenticated maintenance console.")
    type_text("sysinfo");key("ret");wait_for("DISPLAY: 1024 X 768")
    screenshot("authenticated-recovery")
    assert "wrongpass" not in log.read_text()
    print("PASS: corrupted user init falls back to authenticated kernel recovery", flush=True)

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp);tree = root / "tree"
    with (harness.out / "recovery-image.log").open("w") as image_log:
        subprocess.run(["xorriso", "-osirrox", "on", "-indev", str(harness.iso), "-extract", "/", str(tree)],
                       check=True, stdout=image_log, stderr=image_log)
        kernel = tree / "boot/axiom.elf"
        symbols = subprocess.check_output(["nm", str(kernel)], text=True)
        address = next(int(row.split()[0], 16) for row in symbols.splitlines() if row.endswith(" user_init_start"))
        elf = bytearray(kernel.read_bytes())
        phoff = struct.unpack_from("<Q", elf, 32)[0]
        phsize, phcount = struct.unpack_from("<HH", elf, 54)
        for index in range(phcount):
            kind, flags, offset, virtual, physical, filesz, memsz, align = struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phsize)
            if kind == 1 and virtual <= address < virtual + filesz:
                at = offset + address - virtual
                assert elf[at:at+4] == b"\x7fELF"
                elf[at] = 0
                break
        else:
            raise AssertionError("init symbol is outside loadable kernel data")
        kernel.chmod(0o600);kernel.write_bytes(elf)
        image = root / "broken-init.iso"
        subprocess.run(["xorriso", "-as", "mkisofs", "-R", "-r", "-J", "-b", "boot/limine/limine-bios-cd.bin",
                        "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table",
                        "--efi-boot", "boot/limine/limine-uefi-cd.bin", "-efi-boot-part", "--efi-boot-image",
                        str(tree), "-o", str(image)], check=True, stdout=image_log, stderr=image_log)
    harness.iso = image
    for firmware in ("bios", "uefi"):
        harness.run(firmware, exercise, expected_errors=("[ERROR] user-space init failed; entering authenticated recovery",))
