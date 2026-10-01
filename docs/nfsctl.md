# nfsctl

## Overview

nfsctl is a Windows NFS control and diagnostics utility for:

- NFS mount enumeration and mount/unmount actions
- local NFS export discovery
- remote NFS endpoint probing
- Client for NFS registry inspection and tuning
- account-to-SID identity mapping
- Client for NFS debug flag control

It supports text, JSON envelope output, and delimited output for selected read commands.

## Synopsis

```text
nfsctl.exe <tool-switch> [sub-options] [global-options]
```

## Global options

- `--json`, `-j`: emit JSON envelope output
- `--csv`: emit CSV output for supported read commands
- `--tsv`: emit TSV output for supported read commands
- `--dry-run`: preview write operations without applying changes
- `--confirm`: required for mutating operations
- `--force`: allow overwrite when existing registry value differs
- `--backup-reg [path]`: save existing registry value before registry writes
- `--win32-exit`: return raw Win32 exit codes
- `--timeout-ms <n>`: probe timeout in milliseconds (default `1500`)
- `--op-timeout-ms <n>`: operation timeout for non-probe workflows (`0` disables)
- `--ipv4`: probe with IPv4
- `--ipv6`: probe with IPv6
- `--dual-stack`: probe with dual-stack resolution
- `--quiet`, `-q`: suppress informational output
- `--verbose`: enable verbose logs
- `--trace`: enable trace logs

## Commands

### Mount and resource management

- `--mounts`, `-m`: list active NFS mounts
- `--mount <path> <drive:>`: mount a remote NFS share
- `--umount <drive:>`: unmount a mapped drive

Notes:

- `--mount` and `--umount` require `--confirm` unless `--dry-run` is used.

### Exports and probing

- `--exportfs`, `-e`: list local exports or fallback shares
- `--showmount <host>`, `-s <host>`: probe NFSv3 and/or NFSv4 reachability
  - `--nfsv3`: probe the classic NFSv3 path via portmapper `111` and NFS `2049`
  - `--nfsv4`: probe NFSv4 directly on `2049`
  - `--nfsboth`: probe both modes in one run

### Configuration and diagnostics

- `--nfsstat`, `-st`: read NFS client registry state
- `--nfsconf`, `-c`:
  - `--get <KeyName>`
  - `--set <KeyName> <Value>`
- `--nfsidmap <AccountName>`, `-id <AccountName>`: resolve account to SID
- `--rpcdebug`, `-d`:
  - read current debug flags
  - `--set <bitmask>` to update flags

Notes:

- `--nfsconf --set` and `--rpcdebug --set` require `--confirm` unless `--dry-run` is used.
- For write operations, `--backup-reg` stores pre-change values for rollback reference.

## Output modes

### Text

Default, human-readable command output.

### JSON envelope

`--json` emits this schema:

```json
{
  "ok": true,
  "code": 0,
  "command": "showmount",
  "data": {"...": "..."},
  "error": null
}
```

### CSV/TSV

Supported for read-style queries:

- `--mounts`
- `--nfsstat`
- `--showmount`
- `--nfsconf --get`
- `--rpcdebug` (read)

## Exit codes

Normalized exit behavior (default):

- `0`: success
- `1`: argument/usage errors
- `2`: permission/confirmation/cancellation errors
- `3`: network reachability/service probe failures
- `4`: other failures

Use `--win32-exit` to return raw Win32 values.

## Examples

```text
nfsctl.exe --mounts
nfsctl.exe --mounts --json
nfsctl.exe --showmount 192.168.1.50 --nfsv4 --dual-stack
nfsctl.exe --showmount 192.168.1.50 --dual-stack --timeout-ms 750
nfsctl.exe --nfsstat --csv
nfsctl.exe --nfsconf --get Timeout --json
nfsctl.exe --nfsconf --set Timeout 10 --confirm --backup-reg
nfsctl.exe --rpcdebug --set 3 --confirm --force --backup-reg C:\temp\nfs-backup.txt
nfsctl.exe --mount \\192.168.1.50\srv Z: --dry-run
```
