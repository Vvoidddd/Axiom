# Compatibility policy

Axiom is an experimental OS. A successful build does not imply that a previous
data image, binary, or driver is compatible. Changes to a persisted format must
either migrate a validated old version with a rollback copy or reject it without
writing. Never silently reformat an unknown image.

| Boundary | Current contract | Change rule |
| --- | --- | --- |
| Kernel internals and drivers | Compiled together; no stable binary module ABI | Rebuild all components and run the hardware matrix |
| User syscall ABI | Version 1 in `sdk/include/axiom/syscall.h`; x86-64 static ELF | Append calls; preserve existing numbers, meanings, and error values; incompatible changes require a new ABI version |
| Keyboard polling | ASCII, zero when idle, named session-action values above 255 | Consumers must handle extended values before narrowing to a character |
| Package manifests | Explicit package/ABI versions; bundled manifests validated at boot | Reject unsupported ABI versions; unsigned external installation is not supported |
| Service identities | Named reserved service accounts and existing least-privilege rules | Do not reassign existing UIDs/GIDs or broaden capabilities during migration |
| Configuration | Existing schema-versioned documents with validated keys and backups | Migrate through the configuration loader; retain old data on failure |
| Data disk | `AXDATA01` version 1; 64 MiB; two checked generation slots | Unknown version, invalid label, or no recoverable generation stops restore without overwriting |
| Mutable VFS snapshot | `AXVSNAP1` version 1; bounded `/users` and `/var` records | Validate the whole graph and allocate the replacement before committing; never restore `/system`, `/boot`, or `/apps` |
| Desktop | Kernel-hosted preview; no public app/window-server protocol yet | Do not present internal drawing functions as a stable GUI ABI |

Release changes must state affected boundaries and whether users need a new
ISO, a data migration, or an offline backup. Driver tests must distinguish real
hardware from emulation. Current persistent-write validation targets QEMU AHCI;
VMware SATA use is experimental, and other attached disks remain read-only.

Required checks for changes to persistence include `make test-snapshot`,
`make test-checkpoint`, `python3 tests/data-disk.py`, and BIOS/UEFI independent
boot tests in `tests/persistence.py`. These include malformed metadata,
allocation failure, interrupted commits, old-generation fallback, account
authentication after restart, and file content checks independent of rendering.

Hardware command references: [Intel AHCI 1.3.1](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf)
and [Linux ATA command definitions](https://github.com/torvalds/linux/blob/master/include/linux/ata.h).
