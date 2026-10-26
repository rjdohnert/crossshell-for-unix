# diskinfo

## What it does
Lists Windows physical disks, partitions, and mounted volumes, including filesystem, mount relationships, capacity, and read-only state.

## Options
- `-n`, `--noheadings`: suppress the header row
- `--table`: aligned lsblk-style table output with tree branches and `NAME`,
  `MAJ:MIN`, `RM`, `SIZE`, `RO`, `TYPE`, and `MOUNTPOINTS` columns (default)
- `--csv`: CSV output
- `--json`: JSON array output
- Positional filters: case-insensitive substring matches against name, type, parent disk, filesystem, mount point, size, and read-only state
- `-` as a filter argument: read whitespace-delimited filters from standard input
- `-h`, `--help`: show help
- `-V`, `--version`: show version information

## Notes
- Physical disks and partition layouts require Windows device access; unavailable capacity is `null` in JSON and `n/a` in display output.
- JSON `sizeBytes` is numeric or `null`; `readOnly` is boolean or `null`. The formatted `size` remains available for display.
- Windows volume/mount topology differs from Linux block-device relationships; a volume can span multiple parent disks and can have multiple mount paths.
- Windows does not expose Linux device major/minor numbers, so `MAJ:MIN` is shown as `-` in table output.
- `src/diskinfo/test.ps1` builds the utility and checks tree formatting, escaping, JSON/CSV parsing, topology fields, and `-n` behavior.

## UNIX origin
`diskinfo` provides lsblk-style block-device and mount-point reporting.
