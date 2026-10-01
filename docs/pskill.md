# pskill

## What it does

Terminates running processes by PID or name pattern, with optional process-tree traversal, user filtering, signal mode selection, and dry-run preview.

This implementation provides Unix-style targeting semantics on Windows while mapping termination behavior to Win32 APIs.

## Synopsis

```text
pskill [OPTIONS] <TARGET>...
```

## Target specification

- numeric PID: for example `1234`
- executable name: for example `notepad.exe`
- name pattern/substr: for example `chrome`

Targets can be mixed in one invocation.

## Options

- `-h`, `--help`: show detailed help text
- `-f`, `--force`: force termination path (SIGKILL/TerminateProcess behavior)
- `-t`, `--tree`: terminate process trees (children first, parent last)
- `-e`, `--exact`: require exact executable-name match (with `.exe` auto-handling)
- `-c`, `--case-sensitive`: use case-sensitive name matching (default is case-insensitive)
- `-s`, `--signal <SIG>`: select signal mode (`2`, `9`, `15`, `INT`, `KILL`, `TERM`)
- `-u`, `--user <USER>`: only target processes owned by the specified user
- `-n`, `--dry-run`: print what would be terminated without making changes
- `-v`, `--verbose`: show detailed matching and operation output
- `-q`, `--quiet`: suppress standard informational output

## Signal behavior

- `2` or `INT`: posts `WM_CLOSE` only (graceful request)
- `15` or `TERM` (default): posts `WM_CLOSE`, waits briefly, then falls back to forced termination
- `9` or `KILL`: immediate `TerminateProcess` path

## Exit codes

- `0`: success, all matching processes were handled successfully
- `1`: partial failure, one or more matched processes failed to terminate
- `2`: invalid arguments or missing target(s)
- `3`: no active process matched the provided target criteria
- `4`: debug privilege warning (`SeDebugPrivilege` activation failed)

## Examples

Graceful request for all notepad instances:

```text
pskill notepad
```

Force-kill a process tree with verbose output:

```text
pskill -f -t -v 1234
```

Exact match with explicit signal mode:

```text
pskill -e -s=9 chrome.exe
```

Dry-run with owner filter:

```text
pskill -n --user="JohnDoe" cmd.exe
```

Mixed PID and name targets:

```text
pskill -f 1024 2048 msedge.exe
```

## Notes

- Some protected/system processes cannot be terminated even with elevated rights.
- Tree mode uses bottom-up ordering to reduce orphaned children.
- Use dry-run first for broad patterns to avoid accidental termination.

## UNIX origin

Modeled after Sysinternals and Unix-style process-kill workflows, with POSIX-like targeting and signal semantics adapted for Win32.
