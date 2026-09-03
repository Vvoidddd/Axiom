# Axiom OS Roadmap

This is the living engineering roadmap for taking Axiom from its current
single-kernel terminal into a desktop operating system. Move an item into the
completed section only after it builds cleanly and passes its relevant QEMU
tests on both BIOS and UEFI.

## Completed

- [x] Create a freestanding x86-64 C kernel and linker layout.
- [x] Boot through pinned Limine v12 on legacy BIOS and UEFI.
- [x] Produce a hybrid ISO with the `./all` clean-build workflow.
- [x] Draw a graphical splash, bitmap font, and scrolling framebuffer console.
- [x] Add a staged kernel-module and hardware loading screen.
- [x] Add PS/2 keyboard input, PIT delays, reboot, and CPU halt controls.
- [x] Read the Limine memory map and framebuffer metadata.
- [x] Detect CPU vendor, brand, features, cores, and logical threads.
- [x] Report detected and initially usable RAM.
- [x] Add safe reserved-buffer memory diagnostics and CPU diagnostics.
- [x] Add the initial shell command table and bounded command input.
- [x] Install a Global Descriptor Table and Task State Segment.
- [x] Add an Interrupt Descriptor Table and exception handlers with register dumps.
- [x] Replace polling input with IRQ-driven PIT ticks and IOAPIC/PIC interrupt delivery.
- [x] Add a keyboard event queue, modifiers, releases, special keys, and US/Dvorak layouts.
- [x] Parse ACPI tables for CPU, interrupt-controller, HPET, RTC, and power data.
- [x] Initialize the local APIC and I/O APIC with MADT overrides and PIC fallback.
- [x] Start application processors on guarded stacks and run per-core idle loops.
- [x] Implement an HHDM-backed physical page-frame allocator.
- [x] Implement page-table mapping, guarded exception/AP stacks, and a kernel heap.
- [x] Expand `memtest` to reserve, test, and return free physical pages safely.
- [x] Add serial log levels, assertions, panic handling, and register crash dumps.
- [x] Add RTC time, monotonic uptime, entropy sampling, and ACPI shutdown.
- [x] Add shell history, cursor editing, arguments, paged output, and scripts.
- [x] Add PCI, ACPI, interrupt, disk-controller, uptime, log, and allocator commands.
- [x] Add automated BIOS/UEFI command, heap, fault, SMP, and reboot smoke tests.
- [x] Replace one-shot PCI counting with reusable PCI device, BAR, configuration, and bus-mastering APIs.
- [x] Add a common cached block-device API and deterministic recovery RAM disk.
- [x] Parse primary GPT and MBR partition tables with explicit bounds.
- [x] Add a bounded VFS with files, directories, mount points, descriptors, paths, and error codes.
- [x] Create the initial standard directory tree and private `/users/admin` home.
- [x] Add VFS ownership/mode metadata, links, sparse writes, quotas, rename, and locking checks.
- [x] Add bounded read-only FAT32 traversal, 8.3/VFAT names, directories, and file reads.
- [x] Define and implement the AxiomFS v2 superblock and recoverable metadata journal.
- [x] Add AxiomFS format, consistency-check, interrupted-transaction detection, and repair routines.
- [x] Add Phase 2 shell commands for files, directories, metadata, mounts, disk use, and filesystem checks.
- [x] Add a compiled initramfs image with essential configuration, version, welcome, and application-layout files.
- [x] Boot-test block-cache, full-disk, invalid-path, interrupted-write, and journal-repair behavior on BIOS and UEFI.
- [x] Add read-only AHCI/SATA and NVMe drivers and QEMU hardware-device tests.
- [x] Persist the AxiomFS tree, timestamps, ownership, modes, links, extended attributes, sparse data, and quotas.
- [x] Validate MBR/GPT parsing, damaged metadata repair, cache/device loss, and write-disabled physical disks.
- [x] Add public stable and private daily publishing scripts plus automated stable ISO Releases.

## Phase 1: Stable Terminal Kernel — Completed

Phase 1 was completed and verified on BIOS and UEFI in QEMU. Its individual
deliverables are recorded in the completed section above.

## Phase 2: Storage and Files

- [x] Enumerate PCI devices through legacy configuration space; add PCIe ECAM enumeration when MCFG hardware testing is available.
- [x] Add AHCI/SATA and NVMe block-device drivers, starting with read-only operation.
- [x] Add a block cache and partition parsing for GPT and MBR.
- [x] Implement a virtual filesystem interface with files, directories, and mount points.
- [x] Add an initramfs so essential programs and configuration ship with the kernel.
- [x] Add read-only FAT32 first for interoperability and safe driver validation.
- [x] Persist the AxiomFS file/directory tree, timestamps, ownership, permissions, links, and extended attributes on disk.
- [x] Add an AxiomFS formatter, consistency checker, repair tool, and versioned on-disk format.
- [x] Implement journaled metadata so interrupted writes cannot silently corrupt the disk.
- [x] Add safe read/write support, atomic rename, file locking, sparse files, and storage quotas.
- [x] Define the standard directory layout: `/boot`, `/system`, `/apps`, `/users`, `/tmp`, `/var`, and `/devices`.
- [x] Mount a private home directory at `/users/<name>` for each account.
- [x] Add file-descriptor, path, permission, and error-code APIs.
- [x] Add shell commands such as `ls`, `cd`, `pwd`, `cat`, `mkdir`, `cp`, and `rm`.
- [x] Add `mount`, `unmount`, `df`, `du`, `chmod`, `chown`, `ln`, and filesystem-checking commands.
- [x] Test full disks, invalid paths, damaged metadata, interrupted writes, and removable-drive loss.
- [x] Keep physical filesystem writes disabled by default; writable operations are limited to RAM-backed test storage after recovery tests pass.

## Phase 3: Processes and User Space

- [ ] Define a stable syscall ABI and enter user mode through ring 3.
- [ ] Load static ELF executables from the virtual filesystem.
- [ ] Implement processes, threads, preemptive scheduling, sleep, and termination.
- [ ] Give each process an isolated address space with copy and permission validation.
- [ ] Add pipes, signals or events, shared memory, and basic inter-process communication.
- [ ] Create an `init` process and move the command shell out of the kernel.
- [ ] Build a small C standard library and an Axiom user-space SDK/toolchain.
- [ ] Establish application packages, manifests, versioning, and safe installation rules.

## Phase 3A: Users, Administrators, and Permissions

- [ ] Assign every account a stable user ID and primary group ID; reserve ID 0 for the built-in system administrator.
- [ ] Support standard users, administrator users, service accounts, disabled accounts, and a recovery administrator.
- [ ] Create an installer or first-boot setup that requires creation of the initial administrator account.
- [ ] Store password verifiers with a modern salted, memory-hard password hash; never store plaintext passwords.
- [ ] Build login, logout, lock-screen, password-change, and failed-login throttling flows.
- [ ] Add local account commands and APIs: `useradd`, `userdel`, `usermod`, `passwd`, `groups`, `id`, and `whoami`.
- [ ] Implement users and groups databases with atomic updates, validation, backups, and recovery behavior.
- [ ] Give processes real, effective, and saved user/group identities inherited safely across process creation.
- [ ] Enforce owner/group/other read, write, execute, directory-traversal, and device-access permissions in the kernel.
- [ ] Add file ownership, a configurable creation mask, sticky directories, set-user-ID, and set-group-ID semantics.
- [ ] Prevent ordinary users from reading another user's home directory, credentials, processes, or private settings.
- [ ] Create an `admin` group for accounts allowed to request elevated privileges; membership alone must not silently elevate programs.
- [ ] Add an `elevate` command similar to `sudo` that requires authentication, uses an allowlist policy, and grants narrowly scoped temporary privileges.
- [ ] Require explicit confirmation for destructive administrator operations and expire cached elevation credentials quickly.
- [ ] Prefer capabilities for individual powers such as mounting disks, changing time, managing users, networking, and shutting down.
- [ ] Run system services under dedicated least-privilege service accounts rather than the administrator identity.
- [ ] Add per-user environment variables, executable search paths, preferences, startup tasks, and session storage.
- [ ] Record security-relevant events in a protected audit log: logins, failed logins, account changes, elevation, permission failures, and shutdowns.
- [ ] Add account recovery using offline recovery media or a recovery key without creating a universal backdoor.
- [ ] Test privilege escalation attempts, malformed account databases, revoked admins, locked accounts, and concurrent logins.
- [ ] Document the administrator threat model and clearly distinguish kernel, system, administrator, service, and standard-user privileges.

## Phase 3B: Core System Services

- [ ] Add a service manager that starts, stops, restarts, supervises, and logs user-space services.
- [ ] Define declarative system and per-user configuration formats with safe defaults and schema migration.
- [ ] Add a device manager for discovery, permissions, hotplug events, and stable device names.
- [ ] Add a system logger with rotation, persistent journals, filtering, and administrator-only security records.
- [ ] Implement locale, timezone, hostname, keyboard, power, and display configuration services.
- [ ] Add clean shutdown and reboot coordination so applications flush files before power-off.
- [ ] Add software installation, removal, dependency resolution, signed updates, rollback, and an offline recovery environment.
- [ ] Separate immutable system files from mutable configuration, application data, logs, and user files.
- [ ] Add backup and restore tools for user homes, account data, configuration, and filesystem metadata.
- [ ] Provide safe mode, single-user recovery mode, emergency shell, and booting a previous known-good system version.
- [ ] Define stable kernel, syscall, driver, service, package, and application compatibility policies.
- [ ] Create end-to-end installation tests covering blank disks, upgrades, multiple users, admin recovery, and failed updates.

## Phase 4: Input, Graphics, and Desktop

- [ ] Add USB host-controller support, beginning with xHCI.
- [ ] Add USB HID keyboard and mouse drivers plus a unified input-event API.
- [ ] Introduce a graphics abstraction independent of Limine's boot framebuffer.
- [ ] Implement double buffering, clipping, dirty rectangles, and accelerated blitting paths.
- [ ] Create a compositor/window server in user space with windows, layers, and focus.
- [ ] Define an app-to-window-server IPC protocol and reusable GUI toolkit.
- [ ] Add scalable fonts, Unicode text shaping basics, icons, themes, and DPI handling.
- [ ] Build a desktop shell with wallpaper, panel, launcher, notifications, and settings.
- [ ] Build core apps: terminal, file manager, text editor, image viewer, and system monitor.
- [ ] Add clipboard, drag and drop, keyboard shortcuts, accessibility, and screen capture.
- [ ] Preserve a text-only recovery mode when graphics or user space fails.

## Phase 5: Sound and Media

- [ ] Enumerate audio devices and implement Intel HDA as the first sound driver.
- [ ] Create kernel audio-buffer and timing interfaces with underrun diagnostics.
- [ ] Run a user-space audio server for device mixing, per-app volume, and routing.
- [ ] Define application audio APIs and support WAV playback before compressed formats.
- [ ] Add microphone capture, device selection, mute controls, and notification sounds.
- [ ] Build a simple music and media player after filesystem and audio APIs stabilize.

## Phase 6: Networking and Applications

- [ ] Add an initial virtual NIC driver for QEMU, then common real-hardware drivers.
- [ ] Implement Ethernet, ARP, IPv4, ICMP, UDP, DHCP, DNS, and TCP in tested layers.
- [ ] Add sockets, network configuration, firewall rules, and secure random generation.
- [ ] Port a TLS library only after clocks, entropy, files, and sockets are dependable.
- [ ] Add an application store or repository with signed packages and rollback support.
- [ ] Port or build network apps such as a browser shell, downloader, and chat client.

## Phase 7: Reliability, Security, and Releases

- [ ] Enforce W xor X memory, user/kernel isolation, syscall validation, and stack guards.
- [ ] Add capability or permission boundaries for devices, files, network, and desktop IPC.
- [ ] Fuzz parsers and syscall inputs; test low-memory, disk-error, and device-loss paths.
- [ ] Add reproducible builds, versioned disk images, release notes, and upgrade tooling.
- [ ] Test on multiple CPU vendors, core counts, RAM sizes, resolutions, and real machines.
- [ ] Document driver APIs, syscall ABI, app SDK, debugging, contribution, and recovery.

## Current Next Milestone

Phase 2 is complete. The next milestone is Phase 3: define the syscall ABI,
enter ring 3, load static ELF programs, and move the shell into an isolated
user process before account and administrator features are enabled.
