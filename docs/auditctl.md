# auditctl

## What it does
Displays and updates selected Windows Advanced Audit Policy subcategories using Solaris-style class names.

## Usage
- `auditctl -n | --dry-run <command>`
- `auditctl -s | --status`
- `auditctl -e | --enable <class>`
- `auditctl -d | --disable <class>`
- `auditctl -l | --list-classes`
- `auditctl -h | --help`

## Options
- `-s`, `--status`: show current audit state for supported classes
- `-e`, `--enable <class>`: enable Success and Failure auditing for a class
- `-d`, `--disable <class>`: disable auditing for a class
- `-l`, `--list-classes`: list supported class names and Solaris-style aliases
- `-n`, `--dry-run`: show intended enable/disable changes without applying them
- `-h`, `--help`: show help text

## Classes
- `proc`: Process Creation / Execution (Solaris-style `ex` mapping)
- `ex`: Solaris-style alias for `proc`
- `file`: File System object access (Solaris-style `fc` mapping)
- `fc`: Solaris-style alias for `file`
- `all`: apply the action to all supported classes

## Notes
- Changing audit policy requires Administrator rights.
- `SeSecurityPrivilege` must be enabled to query or modify policy in this build.
- `--dry-run` does not require elevation because it does not apply policy changes.
- `--enable` sets both Success and Failure flags.
- `--disable` sets No Auditing.

## Examples
- `auditctl --status`
- `auditctl --enable proc`
- `auditctl --enable ex`
- `auditctl --enable file`
- `auditctl --enable fc`
- `auditctl --enable all`
- `auditctl --disable file`
- `auditctl --disable all`
- `auditctl --list-classes`
- `auditctl --dry-run --enable all`

## Troubleshooting
- If you get `Failed to enable SeSecurityPrivilege`, run from an elevated terminal (Run as Administrator).

## UNIX origin
A Solaris audit policy control utility that manages audit classes and event selection; this implementation adapts that class-style workflow for Windows Advanced Audit Policy.
