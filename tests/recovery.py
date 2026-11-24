#!/usr/bin/env python3
"""Boot the real recovery console without a framebuffer, on BIOS and UEFI."""
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time

iso = Path(sys.argv[1] if len(sys.argv) > 1 else "axiom.iso").resolve()
out = Path(sys.argv[2] if len(sys.argv) > 2 else "build/recovery-test").resolve()
out.mkdir(parents=True, exist_ok=True)
ovmf = os.environ.get("OVMF") or next((p for p in (
    "/usr/share/OVMF/OVMF_CODE.fd", "/usr/share/OVMF/OVMF_CODE_4M.fd") if Path(p).is_file()), None)
assert ovmf, "OVMF required for UEFI coverage"

for firmware, mode in (("bios", "recovery-serial"), ("uefi", "recovery-serial"), ("bios-no-gpu", ""), ("bios-text", "recovery")):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        conf = root / "limine.conf"
        conf.write_text("timeout: 0\n/Axiom recovery test\n    protocol: limine\n    path: boot():/boot/axiom.elf\n    resolution: 1024x768x32\n    cmdline: " + mode + "\n")
        image = root / "recovery.iso"
        tree = root / "tree"
        with (out / f"{firmware}-image.log").open("w") as image_log:
            subprocess.run(["xorriso", "-osirrox", "on", "-indev", str(iso), "-extract", "/", str(tree)],
                           check=True, stdout=image_log, stderr=image_log)
            config = tree / "boot/limine/limine.conf"
            config.chmod(0o600)
            config.write_text(conf.read_text())
            subprocess.run(["xorriso", "-as", "mkisofs", "-R", "-r", "-J", "-b", "boot/limine/limine-bios-cd.bin",
                            "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table",
                            "--efi-boot", "boot/limine/limine-uefi-cd.bin", "-efi-boot-part", "--efi-boot-image",
                            str(tree), "-o", str(image)], check=True, stdout=image_log, stderr=image_log)
        address = str(root / "serial")
        args = [os.environ.get("QEMU", "qemu-system-x86_64"), "-M", "q35", "-smp", "2",
                "-m", "256M", "-cdrom", str(image), "-boot", "d", "-display", "none",
                "-serial", f"unix:{address},server=on,wait=on"]
        if firmware == "bios":
            disk = root / "data.img"
            subprocess.run([sys.executable, "scripts/create-data-disk.py", str(disk)], check=True, stdout=subprocess.DEVNULL)
            args += ["-drive", f"if=none,id=data,format=raw,file={disk}", "-device", "ide-hd,drive=data,bus=ide.1"]
        if firmware == "uefi":
            args += ["-drive", f"if=pflash,format=raw,readonly=on,file={ovmf}"]
        if firmware == "bios-no-gpu":
            args += ["-vga", "none"]
        transcript = bytearray()
        with (out / f"{firmware}-qemu.log").open("w") as errors:
            process = subprocess.Popen(args, stdout=errors, stderr=errors)
            try:
                deadline = time.monotonic() + 10
                while not Path(address).exists():
                    assert process.poll() is None and time.monotonic() < deadline, "QEMU startup failed"
                    time.sleep(.05)
                with socket.socket(socket.AF_UNIX) as serial:
                    serial.connect(address)
                    serial.settimeout(.2)
                    cursor = 0

                    def wait_for(message):
                        global cursor
                        end = time.monotonic() + 60
                        wanted = message.encode()
                        while wanted not in transcript[cursor:]:
                            assert process.poll() is None, "QEMU exited"
                            assert time.monotonic() < end, f"Missing {message}; see redacted {firmware}.log"
                            try:
                                chunk = serial.recv(4096)
                                assert chunk, "Serial closed"
                                transcript.extend(chunk)
                            except socket.timeout:
                                pass
                        cursor = transcript.index(wanted, cursor) + len(wanted)

                    def send(text):
                        # Pace the emulated 16550 FIFO; no UART receive IRQ is required.
                        for byte in text.encode():
                            serial.sendall(bytes([byte]))
                            time.sleep(.003)

                    def login(name, password):
                        wait_for("Login: ");send(name + "\r\n")
                        wait_for("Password: ");send(password + "\r\n")

                    def command(text, result):
                        wait_for("recovery> ");send(text + "\n");wait_for(result)

                    wait_for("Username: ");send("admin\n")
                    wait_for("Password: ");send("recovery-pass\n")
                    wait_for("Confirm password: ");send("wrong-pass\n")
                    wait_for("Setup failed:")
                    wait_for("Username: ");send("admin\n")
                    wait_for("Password: ");send("recovery-pass\n")
                    wait_for("Confirm password: ");send("recovery-pass\n")
                    wait_for("Press Enter after saving it:")
                    key = re.search(rb"SAVE YOUR RECOVERY KEY OFFLINE: ([A-Z2-9]{32})", transcript)[1].decode()
                    send("\n")
                    login("admin", "wrong-pass");wait_for("Recovery login rejected.")
                    login("admin", "recovery-pass");wait_for("Authenticated maintenance console.")
                    command("sysinfo", "DISPLAY: 1024 X 768" if firmware == "bios-text" else "DISPLAY: SERIAL CONSOLE")
                    command("useradd ordinary ordinary-pass", "CREATED UID")
                    command("touch /system/recovery-write", "ERROR: READ ONLY")
                    wait_for("recovery> ");send("logout\n")
                    login("ordinary", "ordinary-pass");wait_for("Recovery login rejected.")
                    login("recovery", key);wait_for("Authenticated maintenance console.")
                    command("echo recovery-key-ok", "recovery-key-ok\r\n")
                    wait_for("recovery> ");send("logout\n")
                    login("admin", "recovery-pass" + "x" * 80);wait_for("Recovery login rejected.")
                    login("admin", "recovery-pass");wait_for("Authenticated maintenance console.")
                    command("echo final-recovery-ok", "final-recovery-ok\r\n")
                    if firmware == "bios":
                        command("usermod admin disable --confirm", "ACCOUNT UPDATED")
                        command("save", "SESSION SAVED TO DATA DISK.")
                        # Independent cold boot; no RAM state or firmware reset behavior can mask persistence bugs.
                        serial.close();process.terminate();process.wait(timeout=5)
                        if Path(address).exists():Path(address).unlink()
                        process = subprocess.Popen(args, stdout=errors, stderr=errors)
                        deadline = time.monotonic() + 10
                        while not Path(address).exists():
                            assert process.poll() is None and time.monotonic() < deadline
                            time.sleep(.05)
                        serial = socket.socket(socket.AF_UNIX)
                        serial.connect(address);serial.settimeout(.2)
                        reboot_at = cursor = len(transcript)
                        wait_for("AXIOM TEXT RECOVERY")
                        assert b"AXIOM TEXT SETUP" not in transcript[reboot_at:]
                        assert b"SAVE YOUR RECOVERY KEY OFFLINE" not in transcript[reboot_at:]
                        login("admin", "recovery-pass");wait_for("Recovery login rejected.")
                        login("recovery", key);wait_for("Authenticated maintenance console.")
                        command("usermod admin enable --confirm", "ACCOUNT UPDATED")
                    assert b"wrong-pass" not in transcript and b"recovery-pass" not in transcript
                    assert b"KERNEL PANIC" not in transcript
                    assert b"[PASS] RING3 SYSCALL FUZZ 4096 CASES" in transcript
                    print(f"PASS: {firmware} text setup, admin/recovery authentication, standard-user rejection, hidden passwords, CRLF, input bounds, sealed image and logout", flush=True)
                    serial.close()
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill();process.wait()
                # Redact the one-time credential before writing test artifacts.
                clean = re.sub(rb"(SAVE YOUR RECOVERY KEY OFFLINE: )[^\r\n]+", rb"\1[REDACTED]", transcript)
                (out / f"{firmware}.log").write_bytes(clean)
