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
- [x] Add stable and daily branch publishing scripts plus automated stable ISO Releases.
- [x] Launch the public Axiom project website through GitHub Pages from the stable branch.
- [x] Define syscall ABI v1 and execute validated static ELF64 programs at ring 3.
- [x] Give user processes isolated CR3 address spaces with validated user-copy boundaries.
- [x] Preempt user processes and shared-address-space threads with PIT-driven context switching.
- [x] Add process sleep/wake, yield, exit status, zombie state, PID, and TID handling.
- [x] Add bounded pipes, process events, and shared-memory mappings.
- [x] Start a compiled user-space `init` and make its ring-3 shell the primary console.
- [x] Add a freestanding user runtime, shared syscall headers, linker layout, and SDK build template.
- [x] Add validated versioned system/application manifests and a packaged ring-3 sample app.
- [x] Assign stable account UID/GID values and reserve identity 0 for recovery administration.
- [x] Support standard, administrator, service, disabled, and recovery account states.
- [x] Store only salted, bounded memory-hard password verifiers with login throttling.
- [x] Add `useradd`, `userdel`, `usermod`, `passwd`, `groups`, `id`, and `whoami` account APIs and commands.
- [x] Require graphical first-boot creation of the initial administrator account.
- [x] Build graphical login, logout, lock-screen, password-change, and failed-login throttling flows.
- [x] Implement validated users/groups databases with atomic replacement, backups, and corrupt-primary recovery.
- [x] Give processes inherited real, effective, and saved user/group identities.
- [x] Enforce file, directory-traversal, execute, and device-access permissions in the kernel.
- [x] Add ownership, per-user creation masks, sticky directories, and set-user-ID/set-group-ID semantics.
- [x] Isolate user homes, credential data, process listings, and private settings.
- [x] Create an `admin` group whose membership permits requests but never silently elevates programs.
- [x] Add authenticated, allowlisted, narrowly scoped `elevate` capabilities with 60-second expiry.
- [x] Require explicit confirmation for destructive administrator actions and revoke stale capabilities.
- [x] Separate mount, time, user, network, power, audit, device, ownership, service, and package capabilities.
- [x] Create non-interactive least-privilege identities and private homes for system services.
- [x] Add per-user homes, environment variables, paths, locale/timezone preferences, umasks, and startup tasks.
- [x] Add a protected security audit log with credential-bearing serial-command redaction.
- [x] Add a random per-install recovery key, one-time display, verifier-only storage, and rotation.
- [x] Test escalation denial, malformed database fallback, disabled users, admin revocation, expiry, and concurrent logins.
- [x] Document the administrator threat model and all kernel/system/admin/service/user privilege boundaries.

## Phase 1: Stable Terminal Kernel — Completed

Phase 1 was completed and verified on BIOS and UEFI in QEMU. Its individual
deliverables are recorded in the completed section above.

## Phase 2: Storage and Files — Completed

Phase 2 was completed and verified with the BIOS/UEFI recovery matrix plus
emulated AHCI and NVMe hardware tests. Its completed deliverables are recorded
above; physical disks remain read-only while AxiomFS writes stay on test media.

## Phase 3: Processes and User Space — Completed

Phase 3 was completed and verified through the BIOS/UEFI matrix. Axiom now
loads static ELF64 programs from the VFS, runs them in isolated ring-3 address
spaces, preempts processes and threads, provides IPC, starts user-space `init`,
and uses the user-space shell as its primary console. The kernel console remains
available only as a recovery fallback.

## Phase 3A: Users, Administrators, and Permissions — Completed

Phase 3A was completed and verified on BIOS and UEFI. Axiom now has graphical
account setup/login, repeatable lock/logout sessions, inherited process
credentials, protected homes, kernel-enforced Unix-style permissions, groups,
short-lived capability elevation, service identities, audit records, and a
unique recovery key. Negative QEMU tests cover ordinary-user escalation,
cross-home access, missing confirmation, disabled accounts, capability expiry,
live administrator revocation, corrupt account data, and simultaneous login
verification across four processors.

## Phase completion log

- Phase 1 — Stable Terminal Kernel: completed and verified on BIOS and UEFI.
- Phase 2 — Storage and Files: completed on BIOS/UEFI plus AHCI and NVMe fixtures.
- Phase 3 — Processes and User Space: completed on BIOS/UEFI with ring-3, preemption, IPC, init, shell, and packaged-app probes.
- Phase 3A — Users, Administrators, and Permissions: completed on BIOS/UEFI with session, privilege, recovery, isolation, revocation, and concurrent-login probes.

Future phases must be appended here only after a clean build and their BIOS/UEFI
runtime, serial-log, and framebuffer-screenshot checks pass.

## Phase 3B: Core System Services

- [x] Add a service manager that starts, stops, restarts, supervises, and logs user-space services.
- [x] Define declarative system and per-user configuration formats with safe defaults and schema migration.
- [x] Add a device manager for discovery, permissions, hotplug events, and stable device names.
- [x] Add a system logger with rotation, persistent journals, filtering, and administrator-only security records.
- [x] Implement locale, timezone, hostname, keyboard, power, and display configuration services.
- [x] Add clean shutdown and reboot coordination so applications flush files before power-off.
- [ ] Add software installation, removal, dependency resolution, signed updates, rollback, and an offline recovery environment.
- [ ] Separate immutable system files from mutable configuration, application data, logs, and user files.
- [ ] Add backup and restore tools for user homes, account data, configuration, and filesystem metadata.
- [ ] Provide safe mode, single-user recovery mode, emergency shell, and booting a previous known-good system version.
- [ ] Define stable kernel, syscall, driver, service, package, and application compatibility policies.
- [ ] Create end-to-end installation tests covering blank disks, upgrades, multiple users, admin recovery, and failed updates.

Progress log (2026-09-27): the first half is implemented and verified in BIOS
and UEFI QEMU boots. The validation covers fixed-position startup checks,
configuration migration/recovery, journal rotation, synthetic device hotplug,
stable device permissions, supervised service lifecycle, per-user settings, and
coordinated reboot flushing. Phase 3B remains open until its final six items pass.

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

Phase 3B is half complete. The next milestone is its software/package lifecycle,
immutable/mutable system split, backup/restore, recovery modes, compatibility
policy, and full blank-disk/upgrade/failure installation test matrix.
