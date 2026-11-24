# Developing Axiom

Axiom is a freestanding x86-64 kernel with a small static-ELF application SDK.
The desktop currently lives in the kernel. There is no public GUI, socket,
audio, or dynamically loadable driver ABI yet. See [compatibility.md](compatibility.md)
for versioning rules and [security.md](security.md) for security boundaries.

## Build and run

On Linux, or Ubuntu under WSL, install GCC, binutils, make, curl, xorriso,
qemu-system-x86 and OVMF. Run `./all` from the repository root. This clean build
replaces `axiom.iso`; it does not format or overwrite an attached data disk.
`make -j4 iso` instead builds `build/axiom.iso`. The Makefile pins the Limine
release and protocol header commit. It does not yet guarantee reproducible ISO
bytes across toolchains or build times.

```sh
qemu-system-x86_64 -M q35 -smp 4 -m 512M -cdrom axiom.iso -boot d -serial stdio
```

For UEFI, add `-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd`
(use your distribution's actual OVMF path). The default menu entry starts the
desktop after three seconds. Accounts and files are volatile without the optional
prepared data disk described in the README.

## Syscall ABI version 1

The source of truth is `sdk/include/axiom/syscall.h`. User code invokes `int 0x80`
with the call number in RAX and arguments in RDI, RSI, RDX, R10. RAX receives a
signed result. Use `ax_syscall4` in `user/libaxiom.c`; its inline assembly marks
memory and condition codes clobbered. Calls are synchronous except that sleep,
yield and exit can schedule another process.

| Number | Call | Arguments / result |
| --- | --- | --- |
| 0 | `SYS_ABI_VERSION` | Returns 1 |
| 1 | `SYS_GETPID` | Current process ID |
| 2 | `SYS_GETUID` | Current real user ID |
| 3 | `SYS_YIELD` | Yield execution |
| 4 | `SYS_SLEEP` | Milliseconds in RDI; ring-3 limit 10,000; `-22` above limit |
| 5 | `SYS_EXIT` | Exit status in RDI; does not return |
| 6 | `SYS_WRITE` | RDI pointer, RSI byte count, at most 4096; returns bytes or `-14` |
| 7 | `SYS_KEY_POLL` | ASCII, 0 when idle, `AXIOM_KEY_SESSION_LOCK` or `AXIOM_KEY_SESSION_LOGOUT` |
| 8 | `SYS_COMMAND` | NUL-terminated command shorter than 128 bytes; 0 after dispatch or `-14` for invalid input |
| 9 | `SYS_SPAWN` | NUL-terminated executable path shorter than 128 bytes; PID or negative error |
| 10 | `SYS_SERVICE_HEARTBEAT` | Supervised service only; 0 or `-13` |

Kernel handlers validate every page of user buffers. `SYS_COMMAND` is a temporary
shell interface, not a filesystem API: 0 means the command was dispatched, not
that its operation succeeded. Commands print their own errors. Spawn checks
execute permission, loads a validated static ELF, and applies inherited identity,
set-ID mode bits and capability expiry. Applications must check negative results.
Never narrow key results to `char` before checking the named session actions.
There are no syscall wrappers for arbitrary file I/O or GUI windows yet.

## Application SDK

`user/crt0.S` aligns the stack, calls `int main(void)`, and exits with its return
value. `user/linker.ld` places the static executable at `0x400000` with page-aligned
sections. Programs have no libc, dynamic linker, command-line arguments or POSIX
environment. Use the small `ax_*` functions from `user/libaxiom.h`.

```c
#include "libaxiom.h"
int main(void) {
    ax_puts("Hello from an Axiom application.\n");
    return 0;
}
```

To bundle an application, follow `hello`: add a source in `user/`, a link rule and
entry in `USER_BINS`, start/end symbols in `src/user_bins.S`, and installation in
`src/initramfs.c`. Follow `packages/hello.pkg` for the manifest. Registration is
currently explicit in `src/package.c`; a manifest alone does not install code.
Rebuild the ISO and launch the app through `ax_spawn`. External installation,
dependency resolution and signature verification are not implemented.

## Kernel driver contracts

Drivers compile into the kernel; `src/*.c` is discovered by the Makefile. Keep
device state in driver-owned structures and register devices only after probing
and initialization succeed. The PCI interface is `src/pci.h`: enumerate with
`pci_device_at`, inspect class/vendor IDs, decode BARs with `pci_bar_address`, and
enable bus mastering only after DMA state is ready. `vmm_map_mmio` maps device
registers; do not cast a physical BAR into a kernel pointer. Use volatile register
access, hardware timeouts and the controller's documented ordering requirements.

The memory APIs in `src/memory.h` provide page allocation (`pmm_alloc`, zero on
failure), direct-map translation, bounded heap allocation, and guarded stacks.
Heap allocation is not a promise of DMA contiguity or alignment. AHCI and NVMe
are the current reference implementations for controller-owned DMA buffers.
Do not initialize an already-owned AHCI controller again during a rescan: its
existing block objects still refer to the original command buffers.

`block_register` copies a `struct block_device`; the `driver` pointer must remain
valid for the device's lifetime. `read`/`write` callbacks receive device-relative
LBA, sector count and a kernel buffer; return 0 only when the whole operation
completed, otherwise return a negative error. The caller owns the buffer. Drivers
must not retain it after returning. `sync` must report flush failures. Set
`readonly` for unsupported writes and `present=false` when access is no longer
safe. Removal invalidation/replacement of live device objects is not a general
hotplug API; do not reuse a device object for different media.

All filesystem I/O goes through `block_read`, `block_write`, and `block_flush`.
512-byte sectors use a 32-line write-back cache; other sizes bypass it. Successful
cached writes are not durable until flush succeeds. A failed flush retains dirty
data for retry. Multi-sector operations can partially complete on hardware error;
use the checkpoint generation protocol for atomic persistent state. Current
block-cache callers must be serialized; this is not an SMP-safe driver API.

`src/device.h` exposes managed names, permissions and a bounded event queue.
It supplies discovery metadata, not direct user access to arbitrary MMIO or DMA.
New device operations need explicit authorization and validated user buffers.

## Debugging and checks

Serial output records boot stages, commands (credential-bearing commands are
redacted), faults and service events. Start QEMU with `-S -s` to wait for GDB:

```sh
gdb build/axiom.elf
# In GDB:
target remote :1234
break kmain
continue
```

Kernel symbols are in `build/axiom.elf`; application symbols are in
`build/user/*.elf`. User processes have private address spaces, so select the
correct process context before inspecting user virtual memory. Panic output
includes exception vector, error, RIP and registers. Record those with the exact
commit, firmware, RAM, CPU/core configuration and serial log.

Run the checks relevant to a change, with warnings treated as errors:

```sh
make test-block test-vfs test-vfs-atomic test-elf test-input
make test-framebuffer test-picture test-desktop test-snapshot test-checkpoint
python3 tests/recovery.py axiom.iso build/recovery-test
python3 tests/recovery-init.py axiom.iso build/recovery-init-test
python3 tests/desktop.py axiom.iso build/desktop-test
python3 tests/memory.py axiom.iso build/memory-test
```

Host tests use ASan/UBSan. QEMU tests exercise BIOS and UEFI independently.
Each boot also executes `/system/bin/abiprobe` in ring 3 with 4096 deterministic
invalid-syscall/input cases. This checks rejection and continued process identity;
it does not prove every possible syscall sequence safe. ELF and BMP host tests
mutate 1000 headers each. Block-cache tests inject partial failed reads, failed
dirty evictions/flushes and device removal. Memory tests inject allocation failures.
Persistence changes additionally require `tests/data-disk.py` and
`tests/persistence.py`; use disposable images, never a user's only data copy.
Passing QEMU does not establish real-machine support.

## Contributions and recovery

Keep changes focused, with a regression check for new nontrivial behavior. Update
ABI/format versions when contracts change, document compatibility limits and
preserve old data on failure. Put finished TODO items in the top Completed
section; do not mark a larger phase done based on one working component. Include
actual build/test commands and hardware coverage in the change description.

Use [recovery.md](recovery.md) for authenticated text/serial maintenance, data-disk
failures and recovery-key handling. Kernel panics and broken memory/interrupt
initialization still halt: the maintenance shell requires a functioning kernel.
