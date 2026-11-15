# Axiom

**Website:** [vvoidddd.github.io/Axiom](https://vvoidddd.github.io/Axiom/) ·
**Stable downloads:** [GitHub Releases](https://github.com/Vvoidddd/Axiom/releases)

Axiom v0.5 is a small educational x86-64 operating system written in freestanding C. It
boots with Limine on BIOS and UEFI, shows a branded splash and module-loading
screen, discovers system hardware, and opens an interactive graphical console.

The startup screen lists every required subsystem before testing. Each item
begins as red `[ NOT TESTED ]`, changes to yellow `[ TESTING ]`, and becomes
green `[ OK ]` only after passing. A failed critical check remains red and stops
the boot before the console starts.

Phase 2 adds read-only AHCI and NVMe drivers, a coherent cached block layer,
GPT/MBR parsing, bounded read-only FAT32 traversal, a bounded in-kernel VFS,
and the persistent AxiomFS v2 format with metadata journaling. The VFS supplies
descriptors, paths, directories, mounts, enforced ownership and permissions,
sparse writes, quotas, links, locks, and recovery tests. Physical disks remain
read-only by default. Phase 3 adds syscall ABI v1, static ELF64 loading, isolated
ring-3 address spaces, PIT-preempted processes and threads, sleep and lifecycle
management, pipes, events, and shared memory. A compiled user-space `init`
starts the primary ring-3 shell and a packaged sample application. The kernel
console is retained only as a recovery fallback.

The shipped `/boot`, `/system`, and `/apps` trees are sealed read-only after
initramfs loading. This protection covers nested files, aliases, metadata,
rename/delete, and already-open descriptors; even recovery identity 0 cannot
unseal them through the VFS. System settings and account databases live in
`/var/config` (0700), logs in `/var/log`, and user/application data in private
`/users/<name>` homes, including `/users/recovery`. `/tmp` remains writable.
These are RAM-backed runtime trees: this change does not add persistent
installation, disk migration, or signed updates. Updating the shipped image
currently requires rebuilding and rebooting.

Account setup and login show all fields at once, with a blinking text caret,
highlighted focus, visible password masks, and a PS/2 mouse pointer. Click a
field or use Tab/Shift+Tab; Enter advances to the next field and submits from
the last field. Arrow keys, Home/End, Backspace, and Delete edit text. The
recovery-key acknowledgement also has a clickable Continue button. Keyboard
navigation remains available when no PS/2 mouse is detected; USB HID support
is still on the roadmap.

Usernames accept uppercase and lowercase letters, hyphens, and digits after
the first character (2-31 characters total). Names remain case-sensitive:
sign in as `Void` if that is the name you created. Setup retains entered fields
when validation fails and displays errors below the Create Account button.

## Build on Windows with Arch WSL

Install the build and emulator dependencies inside Arch WSL:

```sh
sudo pacman -S --needed base-devel curl git xorriso qemu-system-x86 edk2-ovmf
```

From PowerShell, enter WSL and change to the project directory:

```powershell
wsl -d Arch
cd /mnt/c/Users/tolik/OneDrive/Desktop/Code/Axiom
```

Build the kernel or hybrid BIOS/UEFI ISO:

```sh
make all
make iso
```

For the normal one-command build from a WSL terminal in VS Code, run:

```sh
./all
```

This checks the required command-line tools, performs a clean rebuild, and
moves the complete hybrid ISO to `axiom.iso` beside this README.

When using `./all`, the generated image is `axiom.iso` in the project root.
Direct `make iso` builds `build/axiom.iso`, while the kernel remains at
`build/axiom.elf`. Limine is pinned to v12.7.0; the matching protocol dependency
is pinned to an exact commit and downloaded into `build/` on the first build.

Run with legacy BIOS or UEFI firmware:

```sh
make run-bios
make run-uefi
```

On distributions with a different OVMF path, edit the firmware path in the
`run-uefi` target or invoke QEMU directly.

## Desktop preview

After signing in, Axiom opens a mouse-driven desktop with wallpaper and a bottom
app panel. Click an app or use F1 Home, F2 Terminal, F3 Files, F4 Editor, and F5
Settings. Typing on Home also opens Terminal. The X button hides the current
app; terminal output and the editor draft remain available when you return.

- **Terminal:** the existing ring-3 shell in a bounded, scrolling window.
- **Files:** browse your home or the root tree, enter folders, and open small
  text files. Up/Down selects entries, Enter opens them, and Backspace goes up.
- **Editor:** edit ASCII text under 4096 bytes using Left/Right, Home/End,
  Backspace/Delete, and Enter. Save copy creates the next available
  `note-N.txt` in your home, preserving existing files. Save failures retain the
  draft. Save before opening another file or ending the session.
- **Settings:** switch wallpaper colors and choose US/Dvorak keyboard layouts.
- **Lock / Log out:** return to login. Panel actions ask for a second click if
  the editor has an unsaved draft. Terminal session commands retain their
  immediate behavior, so save your draft first.

This is a first framebuffer desktop with one visible app at a time, hosted in
the kernel around the ring-3 shell. A user-space compositor, independent app
windows, notifications, image viewer, and system monitor remain on the roadmap.
The desktop requires at least 960x600; smaller framebuffers keep the console.
Without a prepared data disk, files and accounts remain RAM-backed and disappear
on reboot. With one, committed state survives booting the ISO again. This is
still a desktop preview, not a full installed daily-use OS.

## Persistent data disk

Create a **new** dedicated 64 MiB raw image; the script refuses existing files:

```sh
python3 scripts/create-data-disk.py axiom-data.img
```

Attach it as a SATA disk while continuing to boot the ISO. For QEMU, add
`-drive if=none,id=data,format=raw,file=axiom-data.img -device ide-hd,drive=data,bus=ide.1`
to the q35 invocation. For VMware, convert the new raw image to a new VMDK with
`qemu-img convert -f raw -O vmdk axiom-data.img axiom-data.vmdk`, then attach that
VMDK as an existing **SATA** disk with the VM powered off. Use a new output name
when converting; do not overwrite a previously used data disk. QEMU BIOS/UEFI
is tested; VMware compatibility still needs a real VMware validation run.

The desktop displays **PERSISTENT DATA DISK** when enabled. Click Save, press F6,
or enter `save` in Terminal before powering off the VM. Editor Save copy and
coordinated `shutdown`/`reboot` also checkpoint. Only committed files survive an
unexpected power loss; an unsaved editor buffer is not a file. Initial account
setup is checkpointed after recovery-key acknowledgement. Keep that key offline.

The image holds two CRC-checked generations of `/users` and `/var`, including
account verifiers, settings, permissions, links, sparse data, and logs. Restore
validates the entire replacement tree before committing it. Corrupt or unknown
images are not reformatted; a prior valid generation is used when possible.
Ordinary disks and NVMe devices remain read-only. Each snapshot is limited to
8 MiB, the existing VFS is limited to 256 nodes, and mounted subtrees or links
whose backing files are outside the saved trees cause saving to fail visibly.
The data image is **not encrypted**; protect it like a disk containing private files.
CRC checks detect accidental corruption; they do not authenticate an image or
backup. Only attach data images and backups you trust.

Power off the VM before offline backup/restore. These commands support **raw**
data images and always create a new destination:

```sh
python3 scripts/data-disk.py inspect axiom-data.img
python3 scripts/data-disk.py backup axiom-data.img backup.img
python3 scripts/data-disk.py restore backup.img restored.img
```

Attach `restored.img` instead of the original to test recovery. For VMDK images,
keep an offline copy of the complete VMDK or convert a copy to raw first. See
[compatibility policy](docs/compatibility.md) for format and migration rules.

## Console

The console supports a fixed 127-character input line, backspace, wrapping,
and scrolling. Available commands are:

- `help` — list commands
- `about` — show the Axiom version and purpose
- `clear` — clear the graphical console
- `echo TEXT` — print text
- `memmap` — display the Limine memory map and totals
- `fbinfo` — display framebuffer metadata
- `sysinfo` — summarize the CPU, topology, RAM, and display
- `cpu` — show CPU brand, vendor, cores, threads, and features
- `ram` — show detected and initially usable memory
- `modules` — show startup module status
- `memtest` — safely test a reserved 256 KiB kernel buffer with five patterns
- `cputest` — run integer and CPUID diagnostics
- `version` — show the kernel version, architecture, and boot protocol
- `acpi` — list ACPI tables and interrupt/timer-controller addresses
- `smp` — show online processors and APIC status
- `irqs` — show timer and keyboard interrupt counters
- `pci` and `disks` — enumerate PCI devices or storage controllers
- `partitions` — list detected GPT and MBR partitions
- `ls`, `cd`, `pwd`, and `cat` — browse and read the VFS
- `touch`, `mkdir`, `cp`, `mv`, and `rm` — modify its writable RAM-backed tree
- `ln`, `chmod`, and `chown` — manage links and file metadata
- `mount`, `unmount`, `df`, and `du` — inspect mounts and space use
- `fsck` — check the live VFS; `mkfs ram0` formats only the disposable test disk
- `fatls DEVICE [PATH]` and `fatcat DEVICE PATH` — browse a detected FAT32 volume read-only
- `storagetest` — test cache I/O, full-disk errors, invalid paths, journal interruption, and recovery
- `hwstoragetest` and `rescan` — test or redetect attached AHCI/NVMe devices
- `ps` — list the bounded process table and scheduler counters
- `proctest` — validate the Phase 3 process model and syscall ABI boundaries
- `packages` — list validated system and application package manifests
- `allocstat` and `heaptest` — inspect and test kernel memory management
- `time`, `uptime`, and `random` — show RTC, monotonic time, and entropy
- `logs` — display the in-memory serial log history
- `journal [LEVEL]` — read the rotated persistent journal, optionally filtered by level
- `services`, `service`, and `servicetest` — inspect, control, and verify supervised user-space services
- `config system|user [KEY]` — inspect versioned system or current-user configuration
- `setconfig user KEY VALUE` — persist a validated per-user setting
- `setconfig system KEY VALUE --confirm` — persist a validated system setting after service-capability elevation
- `settings` — show hostname, locale, timezone, keyboard, display-scale, and power-policy settings
- `devices`, `deviceevents`, and `devicetest` — inspect stable device names, hotplug history, permissions, and discovery tests
- `layout us|dvorak` — select and persist the current user's keyboard layout
- `powerstatus` — inspect shutdown/reboot coordinator state
- `run CMD;CMD` and `script demo` — execute command scripts
- `shutdown` — save state, stop services, check filesystems, flush storage, and request ACPI soft power-off
- `reboot` — perform the same coordinated flush before resetting through the PS/2 controller
- `halt` — disable interrupts and halt the CPU

The physical-core count is derived from CPUID topology and the logical-thread
count reported by Limine. On unusual multi-socket or virtualized systems it is
an estimate. `memtest` tests the reserved kernel buffer and pages temporarily
allocated from the physical memory manager, then returns those pages.

## Project layout

- `src/kernel.c` contains Limine requests, startup, and the command table.
- `src/framebuffer.c` and `src/console.c` provide graphical output.
- `src/hardware.c` provides PIT delays, PS/2 input, reboot, and halt.
- `src/system.c` provides CPUID discovery and safe CPU/RAM diagnostics.
- `src/arch.c` and `src/interrupts.S` provide GDT/TSS, IDT, PIC/APIC, and exceptions.
- `src/memory.c` provides physical pages, virtual mappings, guarded stacks, and heap allocation.
- `src/acpi.c`, `src/smp.c`, and `src/pci.c` provide firmware, multiprocessor, and PCI discovery.
- `src/block.c` provides cached block I/O and GPT/MBR discovery.
- `src/storage.c`, `src/ahci.c`, and `src/nvme.c` discover storage and implement read-only hardware I/O.
- `src/vfs.c` provides the file, directory, descriptor, path, and mount API.
- `src/process.c` provides the Phase 3 syscall/process/scheduler foundation.
- `src/account.c` and `src/security.h` provide accounts, groups, capabilities,
  recovery-key authentication, and protected audit records. See
  [docs/security.md](docs/security.md) for the threat model.
- `src/config.c` provides schema-versioned system and per-user settings, safe
  defaults, atomic updates, migration, and backup recovery.
- `src/service.c`, `src/device.c`, and `src/log.c` provide service supervision,
  stable device discovery/hotplug events, and a rotated persistent journal.
- `src/power.c` coordinates account saves, process quiescing, service stops,
  filesystem validation, and block-cache flushes before power transitions.
- `src/elf.c` validates and maps static x86-64 ELF programs from the VFS.
- `src/ipc.c` provides bounded pipes, process events, and shared memory.
- `src/package.c` validates ABI-compatible package manifests.
- `user/` contains user-space `init`, shell, sample app, runtime, and linker layout.
- `sdk/` contains the shared syscall ABI and reusable application build template.
- `src/fat32.c` implements bounded read-only FAT32 directories, VFAT names, and file reads.
- `src/axiomfs.c` implements AxiomFS v2 persistence, journaling, checking, and repair.
- `linker.ld`, `limine.conf`, and `Makefile` define the kernel and boot image.

## Physical hardware

The ISO is hybrid bootable and can be written to removable media, but QEMU is
the supported v0 test target. Writing an image destroys the previous contents
of the selected device. Confirm the exact device path with `lsblk` and keep
backups before using tools such as `dd`; never guess a device name. PS/2 input
may be exposed through USB legacy emulation on real firmware and is therefore
best-effort in this milestone.

## Clean generated files

```sh
make clean      # objects, kernel, ISO, staging tree
make distclean  # also downloaded Limine files
```

## Automated kernel tests

Run the host VFS regression check with address and undefined-behavior sanitizers:

```sh
make test-vfs
make test-input
make test-desktop
make test-snapshot
make test-checkpoint
python3 tests/data-disk.py
```

It checks immutable trees, read-only mounts, aliases, metadata, open descriptors,
recursive deletion, rename cycles, and writable configuration/user data. Boot
also checks the actual sealed initramfs before launching user programs.

The account-input check uses Python 3's standard library and QEMU QMP to click
fields out of order, verify caret/mask pixels, create an account, and sign in
on BIOS and UEFI. It saves framebuffer screenshots and serial logs:

```sh
python3 tests/account-input.py build/axiom.iso
python3 tests/desktop.py build/axiom.iso build/desktop-test
python3 tests/persistence.py build/axiom.iso build/persistence
```

Set `OVMF` if your firmware is not at `/usr/share/OVMF/OVMF_CODE.fd`.

The smoke suite boots QEMU through BIOS and UEFI and validates command scripts,
heap allocation/free, four-processor startup, reboot, deliberate exception
register dumps, cached block I/O, quota/full-disk behavior, and AxiomFS journal
recovery:

```sh
make iso
./tests/smoke.sh build/axiom.iso
```

The suite requires `qemu-system-x86_64` and OVMF. The `fault` command used by
the suite deliberately invokes an invalid opcode and should only be used when
testing panic diagnostics.

Authenticated session and privilege-boundary tests are separate so their
screenshots and audit traces remain easy to inspect:

```sh
./tests/session.sh axiom.iso build/session
./tests/security.sh axiom.iso build/security
./tests/services.sh axiom.iso build/services
./tests/phase3b-half.sh axiom.iso build/phase3b-half
```

Real emulated AHCI/NVMe devices are checked separately:

```sh
./tests/hardware-storage.sh build/axiom.iso
```

This covers AHCI in BIOS and UEFI and NVMe in UEFI. SeaBIOS stalls before the
kernel when an NVMe device is present in this QEMU fixture, so NVMe BIOS boot is
not claimed as verified.

## Publishing stable and daily builds

The single public `Vvoidddd/Axiom` repository uses a protected-purpose `stable`
branch for tested releases and a `daily` branch for ongoing work. GitHub applies
visibility to the whole repository, so both branches are public. With the
repository configured as the `origin` remote, use:

```sh
./scripts/publish-daily.sh
./scripts/publish-stable.sh v0.5.0
```

Stable tags have the form `stable-vMAJOR.MINOR.PATCH`. Pushing one triggers the
stable-release workflow, performs a clean build, and uploads `axiom.iso` plus
its SHA-256 checksum to a GitHub Release. The build workflow also preserves an
ISO artifact for pushes to `daily` and `stable`. Daily snapshots never update
the stable branch; only `publish-stable.sh` does that.
