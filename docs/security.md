# Axiom privilege and account model

Axiom separates identity from authority. A process always has real, effective,
and saved user and group IDs. Child processes and threads inherit those IDs;
set-user-ID and set-group-ID executable bits may change only the effective and
saved IDs. The VFS uses the effective IDs, supplementary groups, creation mask,
and active capability set for every access check.

## Trust levels

- The kernel runs in ring 0 and is the final authority. A kernel compromise is
  a complete system compromise.
- Recovery identity 0 is reserved for repair. It uses a random per-install key
  shown once during setup. Only its salted memory-hard verifier is retained;
  Axiom has no universal recovery password.
- System services use dedicated non-interactive accounts and private data
  directories. A service account is not an administrator.
- Members of `admin` may request elevation, but group membership grants no
  capability by itself. `elevate CAPABILITY PASSWORD` authenticates again and
  grants one allowlisted capability to the calling process for 60 seconds.
- Standard users receive no administrative capabilities. Their home directory,
  processes, settings, and credentials are hidden from other standard users.

The allowlisted capabilities are `mount`, `time`, `users`, `network`, `power`,
`audit`, `device`, `chown`, `dac`, `service`, and `package`. Capabilities are
cleared at logout, on expiry, or when the account loses administrator status.
Destructive account, power, recovery-key, and formatting operations additionally
require a literal `--confirm` argument.

## Files and auditing

The kernel enforces owner/group/other read, write, execute, and directory-search
bits. It also enforces supplementary groups, per-user umasks, sticky-directory
deletion rules, set-group-ID directory inheritance, set-user-ID/set-group-ID
execution, ownership changes, and a separate device capability gate. `/tmp` is
sticky; home directories and account data are mode `0700`/`0600`.

Security events are appended to `/var/log/security.log`, which is readable only
through the `audit` capability. Login success/failure/throttling, account and
group changes, elevation, permission denials, recovery-key rotation, and power
events are recorded. Credential-bearing command arguments are redacted from the
serial command log.

## Threat assumptions and current limits

Axiom v0 is a single-machine hobby kernel. Physical access, malicious kernel
drivers, DMA-capable hardware, and attacks against the kernel itself remain in
the trusted-computing boundary. The current password verifier is deliberately
memory-hard and salted but has not received cryptographic review. There is one
interactive graphical seat; authentication operations are serialized so future
remote seats can be added without racing the account database. W^X enforcement,
driver isolation, signed updates, secure boot, networking, and desktop IPC
boundaries are tracked in later roadmap phases and must not be inferred from the
Phase 3A account boundary.

## Recovery procedure

Store the setup recovery key offline. At the graphical login screen, use account
name `recovery` and that key. Identity 0 can then reset an account with
`passwd USER NEW_PASSWORD --confirm`. Rotate a disclosed key after elevating the
`users` capability with `recoverykey rotate --confirm`; the replacement is shown
once and its plaintext is erased from kernel memory after display.
