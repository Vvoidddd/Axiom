# Axiom user-space SDK

The Phase 3 SDK builds static x86-64 ELF executables for syscall ABI v1. Apps
include `axiom/syscall.h`, link `user/libaxiom.o` and `user/crt0.o`, and use
`user/linker.ld`. Executables are freestanding: no host libc or dynamic loader
is available. The kernel rejects malformed ELF files, non-x86-64 images,
unsupported ABI versions, non-absolute executable paths, and oversized user
pointers.

System package manifests use the documented format in `packages/FORMAT.md`.
Increment the ABI only for incompatible syscall changes.
