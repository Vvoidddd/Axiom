#!/usr/bin/env python3
"""Exercise real PS/2 clicks and account fields through QEMU's QMP interface."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

iso = Path(sys.argv[1] if len(sys.argv) > 1 else "build/axiom.iso").resolve()
out = Path(sys.argv[2] if len(sys.argv) > 2 else "build/account-input").resolve()
out.mkdir(parents=True, exist_ok=True)
ovmf = os.environ.get("OVMF", "/usr/share/OVMF/OVMF_CODE.fd")


def run(firmware):
    log = out / f"{firmware}.log"
    with tempfile.TemporaryDirectory() as tmp:
        address = str(Path(tmp) / "qmp")
        args = [os.environ.get("QEMU", "qemu-system-x86_64"), "-M", "q35", "-smp", "4",
                "-m", "512M", "-cdrom", str(iso), "-boot", "d", "-display", "none",
                "-serial", f"file:{log}", "-qmp", f"unix:{address},server=on,wait=off", "-no-reboot"]
        if firmware == "uefi":
            args += ["-drive", f"if=pflash,format=raw,readonly=on,file={ovmf}"]
        with (out / f"{firmware}-qemu.log").open("w") as errors:
            process = subprocess.Popen(args, stdout=errors, stderr=errors)
            try:
                deadline = time.monotonic() + 10
                while not Path(address).exists():
                    assert process.poll() is None, "QEMU exited"
                    assert time.monotonic() < deadline, "QMP startup timed out"
                    time.sleep(.05)
                with socket.socket(socket.AF_UNIX) as sock:
                    sock.settimeout(10)
                    sock.connect(address)
                    stream = sock.makefile("rwb", buffering=0)
                    stream.readline()

                    def qmp(command, arguments=None):
                        stream.write((json.dumps({"execute": command, "arguments": arguments or {}}) + "\n").encode())
                        while True:
                            reply = json.loads(stream.readline())
                            if "error" in reply:
                                raise AssertionError(reply)
                            if "return" in reply:
                                return reply["return"]

                    qmp("qmp_capabilities")

                    def hmp(command):
                        return qmp("human-monitor-command", {"command-line": command})

                    def wait_for(message):
                        end = time.monotonic() + 45
                        while message not in log.read_text():
                            assert process.poll() is None, "QEMU exited"
                            assert time.monotonic() < end, f"Missing {message}: {log.read_text()[-1500:]}"
                            time.sleep(.05)
                        time.sleep(.12)

                    def key(name):
                        hmp(f"sendkey {name} 30")
                        time.sleep(.06)

                    def type_text(text):
                        for char in text:
                            key("shift-1" if char == "!" else "shift-" + char.lower() if char.isupper() else char)

                    position = [512, 384]

                    def click(x, y):
                        while position != [x, y]:
                            dx = max(-60, min(60, x - position[0]))
                            dy = max(-60, min(60, y - position[1]))
                            hmp(f"mouse_move {dx} {dy}")
                            position[0] += dx
                            position[1] += dy
                            time.sleep(.08)
                        hmp("mouse_button 1"); time.sleep(.08)
                        hmp("mouse_button 0"); time.sleep(.08)

                    def screenshot(name):
                        path = out / f"{firmware}-{name}.ppm"
                        qmp("screendump", {"filename": str(path)})
                        return path.read_bytes()

                    def pixel(ppm, x, y):
                        magic, dimensions, maximum, data = ppm.split(b"\n", 3)
                        assert magic == b"P6" and maximum == b"255"
                        width, height = map(int, dimensions.split())
                        assert x < width and y < height
                        start = (y * width + x) * 3
                        return data[start:start + 3]

                    wait_for("[FORM] account setup ready")
                    assert "PS/2 mouse online" in log.read_text()
                    initial = screenshot("setup")
                    assert pixel(initial, 250, 193) == bytes.fromhex("4f8fe8"), "No initial focus border"
                    assert pixel(initial, 514, 390) == b"\xff\xff\xff", "No mouse pointer"
                    key("left")  # Reset blink; caret must be visible even before typing.
                    assert pixel(screenshot("caret"), 265, 203) == bytes.fromhex("f4f7fa"), "No caret"

                    # Fill fields out of order, proving that clicks actually change focus.
                    click(280, 310); type_text("axiomtest123!")
                    masked = screenshot("password")
                    assert pixel(masked, 265, 308) == bytes.fromhex("f4f7fa"), "Password mask not drawn"
                    click(280, 410); type_text("axiomtest123!")
                    click(280, 210); type_text("0Voix"); key("backspace"); type_text("d")
                    key("tab"); key("shift-tab")
                    click(500, 495)
                    rejected = screenshot("invalid-name")
                    assert pixel(rejected, 250, 524) == bytes.fromhex("ff667b"), "No specific validation message"
                    assert pixel(rejected, 250, 475) == bytes.fromhex("315e97"), "Error overlaps the button"
                    assert pixel(rejected, 265, 308) == bytes.fromhex("f4f7fa"), "Rejected form lost password"
                    assert "first administrator account created" not in log.read_text()
                    key("home"); key("delete")  # Correct 0Void to Void without retyping passwords.
                    screenshot("filled")
                    click(500, 495)
                    wait_for("first administrator account created")
                    time.sleep(1.5)
                    position[:] = [512, 384]  # Each new form starts its pointer at the center.
                    click(500, 450)
                    wait_for("[FORM] login ready")
                    position[:] = [512, 384]
                    click(280, 350); type_text("axiomtest123!")
                    click(280, 250); type_text("Void")
                    screenshot("login")
                    click(500, 450)
                    wait_for("graphical login successful")
                    wait_for("launching user-space init and shell")
                    time.sleep(1)
                    screenshot("terminal")
                    assert "[ERROR]" not in log.read_text()
                    qmp("quit")
                    process.wait(timeout=5)
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
    print(f"PASS: {firmware} mouse signup/login, caret, masks, editing and keyboard focus", flush=True)


for mode in ("bios", "uefi"):
    run(mode)
