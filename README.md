# Axiom

Axiom v0.4 is a small educational x86-64 kernel written in freestanding C. It
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
read-only by default. Phase 3 development has begun on `daily` with syscall ABI
v1, a bounded process table, identity/state tracking, sleep/yield/exit semantics,
and scheduler accounting. A validated static ELF64 program is loaded from the
VFS, mapped into a separate CR3, executed at CPL3, and returned through the
`int 0x80` gate. Bounded pipes, events, and shared memory are also tested. Full
context switching and the interactive user shell are still in progress.

## Build on Windows with Arch WSL

Install the build and emulator dependencies inside Arch WSL:

```sh
sudo pacman -S --needed base-devel curl git xorriso qemu-system-x86 edk2-ovmf
```

From PowerShell, enter WSL and change to the project directory:

```powershell
wsl -d Arch
cd /mnt/c/Users/tolik/OneDrive/Desktop/Axiom
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
- `allocstat` and `heaptest` — inspect and test kernel memory management
- `time`, `uptime`, and `random` — show RTC, monotonic time, and entropy
- `logs` — display the in-memory serial log history
- `layout us|dvorak` — select the keyboard layout
- `run CMD;CMD` and `script demo` — execute command scripts
- `shutdown` — request ACPI soft power-off
- `reboot` — request a reset through the PS/2 controller
- `halt` — disable interrupts and halt the CPU

The physical-core count is derived from CPUID topology and the logical-thread
count reported by Limine. On unusual multi-socket or virtualized systems it is
an estimate. `memtest` intentionally does not overwrite arbitrary free RAM;
doing that safely requires the future physical memory manager.

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
- `src/elf.c` validates and maps static x86-64 ELF programs from the VFS.
- `src/ipc.c` provides bounded pipes, process events, and shared memory.
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
