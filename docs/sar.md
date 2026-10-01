# sar

## What it does

Reports system activity in an HP-UX `sar`-style layout for Windows. It can sample CPU, memory, disk, paging, kernel tables, per-core CPU usage, network activity, and queue depth in human-readable or machine-readable output.

## Usage

- `sar.exe [options] [interval [count]]`

## Options

- `-u`: report CPU utilization (`%usr`, `%sys`, `%wio`, `%idle`)
- `-r`: report memory and swap usage (`freeKB`, `swapKB`, `%used`, `%swp`, `totKB`)
- `-d`: report aggregate disk activity (`%busy`, `avq`, `r+w/s`, `blks/s`, `avwait`)
- `-D`: report per-device disk activity, one row per disk instance
- `-v`: report kernel table activity (processes, threads, handles)
- `-M`: report per-core CPU utilization
- `-b`: report block-device transfer statistics in sar-style format
- `-n`: report network interface activity (rxpck/s, txpck/s, rxkB/s, txkB/s)
- `-q`: report run queue and active task information
- `-w`: report paging activity (pages in/out per second)
- `-A`: report all supported metrics in a single output pass
- `--csv`: emit CSV rows for scripting and log capture
- `--json`: emit JSON rows for scripting and log capture
- `-h`, `--help`, `/ ?`: show help text

## Notes

- If no report flag is supplied, `sar` defaults to CPU-only output.
- `-A` is the most complete collection mode and formats the sections in a compact, readable multi-block layout.
- This is a Windows-native approximation of the HP-UX utility and is meant to be sar-like rather than a strict byte-for-byte clone.
- Data sources are Windows performance counters, PDH, and networking APIs.

## Examples

- `sar.exe 1 5`
- `sar.exe -r 2 10`
- `sar.exe -d 1 0`
- `sar.exe -w 3`
- `sar.exe -v 2 5`
- `sar.exe -M --csv 1 3`
- `sar.exe -A 1 3`

## UNIX origin

The original `sar` utility comes from the System V / AIX / HP-UX performance-monitoring toolset and is widely used to view system activity over time.
