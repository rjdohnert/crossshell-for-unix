# pthctl

## What it does
`pthctl` is a Windows PATH environment variable manager. It can list entries, check whether a path exists, add new entries, remove entries, and clean duplicate entries from either the User or System PATH.

It updates PATH through the Windows registry and notifies running applications when changes are written.

## Commands
- `list` (alias: `ls`) : Display current PATH entries
- `add <path>` : Add a directory entry to PATH
- `remove <path>` (alias: `rm`) : Remove a directory entry from PATH
- `check <path>` : Check if a directory entry exists in PATH
- `clean` : Deduplicate and clean up PATH entries

## Options
- `-u`, `--user` : Target User PATH variable (default)
- `-s`, `--system` : Target System PATH variable (requires Administrator privileges)
- `-p`, `--prepend` : Add entry to beginning of PATH (default is append)
- `-d`, `--dry-run` : Preview changes without writing to registry
- `-h`, `--help` : Show help and exit
- `-v`, `--version` : Show version and copyright information

## Notes
- `--prepend` is valid only with `add`.
- `--dry-run` is valid only with `add`, `remove`, and `clean`.
- `--user` and `--system` are mutually exclusive in a single command.
- Modifying System PATH requires elevated privileges.
- `clean` deduplicates by normalized path comparison while preserving first-seen raw entry text.
- Writes are blocked if resulting PATH would exceed Windows size limits, and a warning is shown for very large PATH values.
