# Recovery

**Axiom safe mode** starts the normal authenticated ring-3 shell without the
desktop, bundled sample-app autostart, or background services. Account permissions
and capability elevation still apply. Service start/restart is refused even after
elevation, and logging out and back in keeps safe mode active. Reboot using the
normal Axiom menu entry to restore normal startup. Kernel drivers and required
startup checks still run; this is a reduced user session, not a driver-free boot.
If no framebuffer is available, authenticated serial recovery takes precedence.

`python3 tests/safe-mode.py axiom.iso build/safe-mode-test` validates safe boot,
service-start denial, and repeat login on BIOS and UEFI.

The boot menu offers **Axiom text recovery** and **Axiom serial recovery**.
Select one during the three-second menu timeout. Both run a kernel maintenance
console without starting the desktop or user-space session. Serial recovery
does not initialize a framebuffer; COM1 uses 38400 baud, 8 data bits, no parity,
one stop bit. In QEMU, add `-serial stdio`. With no usable boot framebuffer the
kernel automatically chooses text recovery.

Text mode accepts PS/2 keyboard input and COM1 input. Output goes to the text
console when a framebuffer is available and to COM1. Serial terminal password
input is not echoed; turn off your terminal's local echo. CRLF and LF terminals
are supported. Oversized lines are discarded rather than authenticating a
truncated password or executing a truncated command.

On a fresh live system, create the first administrator and save the one-time
recovery key. On an existing data disk, use an administrator's credentials or
log in as `recovery` with that disk's saved recovery key. Standard and service
accounts cannot enter maintenance mode. This console deliberately grants root
file access after authentication; the sealed `/boot`, `/system`, and `/apps`
trees remain read-only. Do not expose the serial console to untrusted users.
There is no unauthenticated root-shell fallback.
Restoring an existing data disk never reopens first-account setup, even if every
administrator is disabled or has been removed. Use that disk's recovery key to
enable an administrator or create a replacement account.

`help`, `sysinfo`, `disks`, `partitions`, `services`, `journal`, `ls`, `cat` and
the existing account commands are available. Recovery commands use the same
kernel command dispatcher as the desktop terminal. Commands which require
confirmation still require it. `logout`, `lock` or `exit` returns to recovery
authentication; reboot to return to the desktop. Commands that contain a new
password are visible as you type them, unlike the hidden login password prompt.

Use `save` to commit mutable state to an already prepared data disk. With no data
disk the session is RAM-backed and disappears on reboot. Recovery does not
install Axiom, select an older system image or bypass a damaged account database.
If a data disk fails validation, boot stops without overwriting it: power off,
back up the image, then use the README's offline data-disk verification/restore
workflow. Detach that disk to boot a fresh live session, which cannot recover
files from the detached image.

If launching user-space init fails, Axiom stops the desktop and enters this same
authenticated console. A kernel panic, failed required startup self-test, or
broken memory/interrupt subsystem still halts; reboot from a working ISO.
Booting a previous known-good system version and signed update rollback remain
unimplemented and are tracked separately in TODO.

`python3 tests/recovery.py axiom.iso build/recovery-test` exercises framebuffer-free
BIOS and UEFI boots, setup validation, hidden passwords, both administrator and
recovery-key login, ordinary-user rejection, oversized inputs, logout and sealed
system files. It makes temporary test ISOs and leaves the supplied ISO unchanged.
It also tests BIOS with no emulated graphics device and the framebuffer-backed
text console. `tests/recovery-init.py` corrupts the embedded init ELF in a temporary
ISO, then verifies that login reaches authenticated recovery rather than an
unrestricted shell, on both BIOS and UEFI.
The serial BIOS test also disables the only administrator, reboots a real SATA
data image, rejects the disabled account and restores access with the original
recovery key.
