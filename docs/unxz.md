# unxz

## What it does
Decompresses `.xz` files through an external xz-compatible backend.

## Options
- `-k`, `--keep`: keep input files
- `-f`, `--force`: overwrite output files
- `-h`, `--help`: show help text
- `--version`: show version information

## Notes
- Baseline implementation is a backend wrapper and invokes backend xz with `-d`.
- Backend resolution order:
  - local `xz-backend.exe`
  - local `busybox.exe xz`
  - external `xz.exe` on `PATH` (excluding self-recursion)

## UNIX origin
`unxz` is the canonical Unix companion command used to decompress `.xz` files.
