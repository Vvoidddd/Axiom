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
    assert pixel(screenshot("initial-home"), 40, 70) == bytes.fromhex("294963"), "No desktop icon focus"
    click(100, 570)
    wait_for("[DESKTOP] Task Manager")
    tasks = screenshot("task-manager")
    assert pixel(tasks, 32, 64) == bytes.fromhex("293b50"), "No Task Manager window"
    time.sleep(1.2)
    screenshot("task-manager-refresh")
    key("f1"); key("home"); key("tab"); key("ret")
    wait_for("[DESKTOP] Files")
    key("f1"); click(100, 100)
    wait_for("[DESKTOP] Terminal")

    def command(text):
        # The existing harness types simple names; commands also need spaces/slashes.
        for c in text:
            key({" ": "spc", "/": "slash", ".": "dot", "-": "minus"}.get(c, "shift-" + c.lower() if c.isupper() else c))
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

    key("f8")
    for _ in range(len("/users/Void/document.txt")): key("backspace")
    for c in "/users/Void/work.txt": key({"/":"slash", ".":"dot"}.get(c, "shift-v" if c=="V" else c))
    screenshot("save-as-path")
    key("ret"); wait_for("[DESKTOP-SAVED] /users/Void/work.txt")
    command_text(" updated", key)
    key("f8")
    for _ in range(len("/users/Void/document.txt")): key("backspace")
    for c in "/users/Void/work.txt": key({"/":"slash", ".":"dot"}.get(c, "shift-v" if c=="V" else c))
    key("ret"); screenshot("replace-confirmation")
    before = log.read_text().count("[DESKTOP-SAVED] /users/Void/work.txt")
    assert before == 1, "Overwrite skipped confirmation"
    key("ret")
    assert log.read_text().count("[DESKTOP-SAVED] /users/Void/work.txt") == 2
    screenshot("replaced-file")

    # Separate windows retain positions and focus, with real PS/2 dragging.
    key("f1"); key("f2"); key("f3")
    click(200, 80); hmp("mouse_button 1"); time.sleep(.1)
    for _ in range(2):
        hmp("mouse_move 50 30"); position[0] += 50; position[1] += 30; time.sleep(.15)
    hmp("mouse_button 0"); time.sleep(.2)
    windows = screenshot("overlapping-windows")
    assert pixel(windows, 32, 64) == bytes.fromhex("25303c"), "Background Terminal not visible"
    assert pixel(windows, 132, 124) == bytes.fromhex("293b50"), "Files did not move"
    click(60, 80); time.sleep(.15)
    assert pixel(screenshot("window-focus"), 32, 64) == bytes.fromhex("293b50")
    click(950, 200); key("f10"); time.sleep(.15)
    assert pixel(screenshot("maximized"), 8, 42) == bytes.fromhex("293b50")
    key("f10"); time.sleep(.15)
    assert pixel(screenshot("restored-window"), 132, 124) == bytes.fromhex("293b50")
    key("f11"); time.sleep(.15)
    assert pixel(screenshot("minimized"), 950, 230) == bytes.fromhex("1b3954")
    key("f3"); click(974, 139)  # Close Files; its app state can still be reopened.

    key("f5")
    click(245, 180)  # Slate wallpaper
    key("f1")
    assert pixel(screenshot("slate"), 10, 80) == bytes.fromhex("242936")
    key("f2")
    # Existing VFS permissions remain in force, including sealed image writes.
    command("touch /system/desktop-test")
    chrome("sealed-write-denied")
    click(725, 745)
    screenshot("taskbar-tasks")
    key("f1"); key("f7")
    screenshot("shortcut-tasks")
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
    print("PASS: desktop icons, keyboard launch, Task Manager, terminal, files, editor, settings and sessions", flush=True)


def command_text(text, key):
    for c in text:
        key("spc" if c == " " else c)


for mode in ("bios", "uefi"):
    harness.run(mode, exercise)
