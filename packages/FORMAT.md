# Axiom package manifest v1

Each UTF-8 manifest contains one `key=value` pair per line:

```text
name=shell
version=0.1.0
abi=1
type=system
executable=/system/bin/shell
```

Names contain lowercase ASCII letters, digits, `_`, or `-`. Versions use three
numeric components. Phase 3 accepts ABI 1 system executables beneath `/system`
and application executables beneath `/apps`; each target must already exist in
the immutable initramfs. Runtime installation and replacement are disabled.
