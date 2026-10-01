# xz

## What it does
Compresses files using `.xz` format through an external xz-compatible backend.

## Options
- `-d`, `--decompress`: decompress mode
- `-k`, `--keep`: keep input files
- `-f`, `--force`: overwrite output files
- `-h`, `--help`: show help text
- `--version`: show version information

## Notes
- Baseline implementation is a backend wrapper.
- Backend resolution order:
  - local `xz-backend.exe`
  - local `busybox.exe xz`
  - external `xz.exe` on `PATH` (excluding self-recursion)

## UNIX origin
`xz` is the standard Unix utility for LZMA2/XZ compression.
