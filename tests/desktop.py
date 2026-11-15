#!/usr/bin/env python3
"""Real PS/2 desktop actions, file round trips, clipping and session controls."""
import importlib.util
from pathlib import Path
import time

spec = importlib.util.spec_from_file_location("account_input", Path(__file__).with_name("account-input.py"))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


def exercise(qmp, hmp, wait_for, key, type_text, click, screenshot, pixel, position, log):
    wait_for("[DESKTOP] session ready")
    assert pixel(screenshot("initial-home"), 40, 70) == bytes.fromhex("152d46"), "Login did not open Home"

    def command(text):
        # The existing harness types simple names; commands also need spaces/slashes.
        for c in text:
            key({" ": "spc", "/": "slash", ".": "dot", "-": "minus"}.get(c, c))
        key("ret")
        time.sleep(.15)

    def chrome(name):
        ppm = screenshot(name)
        assert pixel(ppm, 0, 0) == bytes.fromhex("101c2a"), "Terminal overwrote top panel"
        assert pixel(ppm, 0, 735) == bytes.fromhex("101c2a"), "Terminal overwrote taskbar"
        return ppm

    command("help")
    for _ in range(4):
        command("help")  # Scroll many times without moving the desktop chrome.
    terminal = chrome("terminal-scroll")
    click(55, 745)
    home = chrome("home")
    assert pixel(home, 40, 70) != pixel(terminal, 40, 70), "Home did not hide Terminal"
    click(180, 745)
    restored = chrome("terminal-restored")
    assert pixel(restored, 44, 110) == pixel(terminal, 44, 110), "Terminal content lost"

    key("f4")
    command_text("hello desktop", key)
    key("ret")
    command_text("second line", key)
    screenshot("editor-draft")
    click(75, 745)  # Hiding the editor retains the draft.
    key("f4")
    click(85, 125)
    wait_for("[DESKTOP-SAVED] /users/Void/note-1.txt")
    screenshot("editor-saved")

    key("f3")
    screenshot("files")
    # Also read the saved file through the existing terminal path.
    key("f2")
    command("cat /users/Void/note-1.txt")
    chrome("note-content")
    key("f3")
    click(265, 125)  # Root
    screenshot("root-files")
    click(160, 125)  # Home
    screenshot("home-files")
    key("down"); key("ret")  # .config is first; note-1.txt is second.
    screenshot("reopened-note")
    click(85, 125)
    wait_for("[DESKTOP-SAVED] /users/Void/note-2.txt")

    key("f5")
    click(245, 180)  # Slate wallpaper
    key("f1")
    assert pixel(screenshot("slate"), 10, 80) == bytes.fromhex("242936")
    key("f2")
    # Existing VFS permissions remain in force, including sealed image writes.
    command("touch /system/desktop-test")
    chrome("sealed-write-denied")
    click(830, 745)
    wait_for("session locked; returning to login")
    wait_for("[FORM] login ready")
    position[:] = [512, 384]
    click(280, 250); type_text("Void")
    click(280, 350); type_text("axiomtest123!")
    click(500, 450)
    time.sleep(2)
    assert log.read_text().count("[DESKTOP] session ready") == 2
    assert log.read_text().count("[SERVICE-SPAWN]") == 2, "Unlock restarted healthy services"
    position[:] = [512, 384]
    chrome("unlocked")
    click(950, 745)
    wait_for("session logged out; returning to login")
    print("PASS: desktop terminal, scrolling, apps, draft save, settings, lock and logout", flush=True)


def command_text(text, key):
    for c in text:
        key("spc" if c == " " else c)


for mode in ("bios", "uefi"):
    harness.run(mode, exercise)
