# Axiom OS Roadmap

This is the living engineering roadmap for taking Axiom from its current
single-kernel terminal into a desktop operating system. Move an item into the
completed section only after it builds cleanly and passes its relevant QEMU
tests on both BIOS and UEFI.

## Completed

- [x] Add simultaneously visible desktop windows with independent positions, title-bar dragging, stacking/focus, minimize/maximize/restore, close/reopen, and F10-F12 keyboard controls.
- [x] Composite the desktop into a bounded software backbuffer with clipping and damaged-region presentation; retain a direct-render fallback when surface allocation fails.
- [x] Enforce user-page W xor X and validate complete ELF layouts before allocation: executable entry points, disjoint load pages, stack/guard reservation, alignment, and page limits.
- [x] Release private pages and page tables after failed ELF loads; test allocation/map/read failures, malformed headers, and 1,000 deterministic mutations under sanitizers.
- [x] Roll back partial kernel heap, guarded-stack, MMIO, and page-table allocations, including virtual reservations and empty lower-level tables; verify injected failures repeatedly on BIOS/UEFI.
- [x] Add all-or-nothing VFS content replacement and an editor Save As flow with explicit overwrite confirmation; fault-test preservation of the old file and draft on failed saves.
- [x] Roll back newly allocated VFS write pages on allocation failure; reject offset overflow, honor locks on existing descriptors, and make empty writes allocation-free.
- [x] Add recognizable desktop launcher icons with mouse activation, keyboard focus, Tab/arrows, and Enter/Space navigation.
- [x] Add a live graphical Task Manager showing permitted process rows, states, scheduler ticks, free RAM, heap use, and uptime, with taskbar and F7 access.

- [x] Persist accounts, homes, configuration, logs, ownership, modes, timestamps, links, sparse pages, and quotas on an explicitly prepared SATA data disk.
- [x] Commit bounded mutable-tree snapshots through two CRC-checked generations, flush the device before committing, and fall back to the prior valid generation.
- [x] Validate and stage restored trees before replacing live state; preserve immutable system trees and reject corrupt data without overwriting the disk.
- [x] Add offline raw-data-image backup and restore tools for user homes, account data, configuration, and filesystem metadata, refusing existing destinations.
- [x] Define kernel, syscall, driver, service, package, application, configuration, and persistent-image compatibility policies in `docs/compatibility.md`.
- [x] Add desktop Save/F6, terminal `save`, editor-copy checkpoints, and coordinated power-off checkpoints for prepared data disks.
- [x] Verify fresh account creation, independent BIOS/UEFI boots, authentication, saved file bytes, SATA rescans, and backup/restore round trips.

- [x] Open a first mouse-driven desktop after login with wallpaper, an app panel, and F1-F5 app shortcuts.
- [x] Keep the ring-3 terminal in a bounded window with scrolling and retained output when hidden.
- [x] Browse permitted VFS directories and open small text files from a graphical file browser.
- [x] Edit bounded ASCII documents and save new home-folder copies without overwriting existing files; retain drafts on save failure.
- [x] Add desktop wallpaper/keyboard controls and lock/logout buttons with an unsaved-draft guard.

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
- [x] Add a service manager that starts, stops, restarts, supervises, and logs user-space services.
- [x] Define declarative system and per-user configuration formats with safe defaults and schema migration.
- [x] Add a device manager for discovery, permissions, hotplug events, and stable device names.
- [x] Add a system logger with rotation, persistent journals, filtering, and administrator-only security records.
- [x] Implement locale, timezone, hostname, keyboard, power, and display configuration services.
- [x] Add clean shutdown and reboot coordination so applications flush files before power-off.
- [x] Separate immutable system files from mutable configuration, application data, logs, and user files.
- [x] Add PS/2 mouse input and clickable account setup, login, and recovery-key acknowledgement.
- [x] Show full account forms with a blinking caret, focus borders, password masks, keyboard navigation, and bounded text editing.
- [x] Preserve service heartbeat age across lock/login pauses so unlocking does not restart healthy services.
- [x] Accept mixed-case account names and show specific signup validation errors without clearing fields or covering buttons.

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

- [ ] Add software installation, removal, dependency resolution, signed updates, rollback, and an offline recovery environment.
- [ ] Provide safe mode, single-user recovery mode, emergency shell, and booting a previous known-good system version.
- [ ] Create end-to-end installation tests covering blank disks, upgrades, multiple users, admin recovery, and failed updates.

Progress log (2026-09-27): the first half is implemented and verified in BIOS
and UEFI QEMU boots. The validation covers fixed-position startup checks,
configuration migration/recovery, journal rotation, synthetic device hotplug,
stable device permissions, supervised service lifecycle, per-user settings, and
coordinated reboot flushing. Completed items are listed at the top of this file;
Phase 3B remains open while the tasks above are unfinished.

Progress log (2026-11-14): seal the shipped `/boot`, `/system`, and `/apps`
trees after loading; keep settings/accounts in `/var/config`, logs in `/var/log`,
and application/user data in private homes. Root and elevated capabilities cannot
unseal the image. Host sanitizer tests cover aliases, open descriptors, metadata,
recursive deletion, and read-only mount inheritance. BIOS/UEFI validation covers
the sealed image, ring-3 probes, account setup/login, settings, and service/power
flows. This is a RAM-backed runtime split, not a persistent installer or updater.

## Phase 4: Input, Graphics, and Desktop

- [ ] Add USB host-controller support, beginning with xHCI.
- [ ] Add USB HID keyboard and mouse drivers plus a unified input-event API.
- [ ] Introduce a graphics abstraction independent of Limine's boot framebuffer.
- [ ] Add hardware-accelerated blitting and per-window damage scheduling to the software-buffered drawing path.
- [ ] Create a compositor/window server in user space with windows, layers, and focus.
- [ ] Define an app-to-window-server IPC protocol and reusable GUI toolkit.
- [ ] Add scalable fonts, Unicode text shaping basics, icons, themes, and DPI handling.
- [ ] Extend the initial desktop with notifications, a general application launcher, and full graphical system settings.
- [ ] Move the desktop's multiple app windows into isolated user-space applications; add an image viewer.
- [ ] Add safe task termination with ownership checks, protected system/session processes, and explicit unsaved-work handling.
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

- [ ] Finish kernel W xor X protection and broaden syscall validation, user/kernel isolation, and stack-guard fault tests.
- [ ] Add capability or permission boundaries for devices, files, network, and desktop IPC.
- [ ] Fuzz parsers and syscall inputs; test low-memory, disk-error, and device-loss paths.
- [ ] Add reproducible builds, versioned disk images, release notes, and upgrade tooling.
- [ ] Test on multiple CPU vendors, core counts, RAM sizes, resolutions, and real machines.
- [ ] Document driver APIs, syscall ABI, app SDK, debugging, contribution, and recovery.

## Current Next Milestone

The desktop can now keep committed data across boots on an explicitly prepared
64 MiB SATA image. It still boots from the ISO and is not a full disk installer.
Without that image, the desktop remains a volatile live session. Phase 3B
package/update/recovery work and Phase 4's user-space compositor remain open.

The new phases below turn these foundations into a usable installed system.
Phase 8 has started with the persistent-image and offline backup/restore work
recorded in Completed. New phases do not imply that Phases 3B-7 are finished.

## Phase 8: Installed Systems and Offline Recovery

- [ ] Build an installer that previews the exact target disk, partitions, and data loss before a separately confirmed write; test installing and booting without the ISO on BIOS and UEFI.
- [ ] Replace whole-tree checkpoints with filesystem-backed incremental writes and test sudden power loss during ordinary file saves.
- [ ] Add a boot recovery menu that can select the previous valid data generation without modifying either generation first.
- [ ] Add image inspection/repair tooling that reports damaged records and exports recoverable files to a new image.
- [ ] Add snapshot space accounting and a visible warning before the 8 MiB snapshot or 256-node VFS limits prevent saving.
- [ ] Add supported image resizing and a migration test that preserves accounts, permissions, sparse files, and recovery access.
- [ ] Add encrypted user-data storage only after secure entropy, key derivation, recovery, and power-loss behavior are validated.
- [ ] Support backups/export of a single user's home without exposing another user's files or the account database.

## Phase 9: Workspace and Productivity

- [ ] Retain unsaved documents through screen locking and user switching without exposing one session's buffers to another.
- [ ] Add editor undo/redo, selection, search, line navigation, and bounded large-file handling.
- [ ] Add file associations and Open With based on validated file types rather than executable filename guesses.
- [ ] Restore approved per-user window positions and open documents after reboot, with a way to disable session restore.
- [ ] Add keyboard access to every desktop control, visible focus, scalable text, and high-contrast rendering tests.
- [ ] Add safe host/guest file import and export with explicit destinations and permission checks.

## Phase 10: Hardware Qualification and Maintainable Releases

- [ ] Run a documented VMware SATA/input/display test matrix and distinguish verified VMware versions from QEMU-only results.
- [ ] Add VirtualBox and physical-machine smoke profiles with recorded CPU, firmware, storage, input, and graphics results.
- [ ] Clean up failed process/thread creation and partial shared-memory mappings; test process/address-space teardown before increasing application limits.
- [ ] Add automated old-release data-image migration and rollback fixtures to CI, including corrupt and interrupted upgrades.
- [ ] Build a user-approved diagnostic export that redacts credentials, recovery keys, and private file contents by default.
- [ ] Publish reproducible release manifests, artifact hashes, compatibility notes, and a tested recovery procedure for each release.

## Phase 11: Custom Axiom Bootloader

- [ ] Specify a versioned kernel handoff for memory maps, framebuffer, ACPI, modules, and boot arguments, with bounded validation tests.
- [ ] Build an Axiom UEFI loader that validates and loads ELF64 kernels and handles the ExitBootServices memory-map retry correctly.
- [ ] Add a boot menu, recovery entries, and selectable known-good kernels; keep the existing Limine path available during development.
- [ ] Test malformed kernels, missing files, unsupported graphics, low memory, and failed handoffs on OVMF before real firmware qualification.
- [ ] Add a legacy BIOS loader only after the UEFI loader and recovery path pass the boot matrix.
