# vmstat

## What it does
Displays virtual memory, paging, interrupt, system call, context switch, and CPU activity over time.

On Windows, `vmstat` uses NT performance counters when available and automatically falls back to PDH counters when needed.

## Usage
- `vmstat [options] [interval [count]]`

## Options
- `-h`, `--help`: show help
- `-v`, `--version`: show version
- `-s`, `--summary`: show summary counters and exit
- `-H`, `--human`: memory values in auto units (`B/K/M/G`)
- `-M`, `--megabytes`: memory values in MB
- `-K`, `--kilobytes`: memory values in KB (default)
- `-n`, `--lines <number>`: repeat table header every N printed lines
- `--output <mode>`: output mode `table`, `csv`, or `json`
- `--csv`: shortcut for `--output csv`
- `--json`: shortcut for `--output json`
- `--layout <mode>`: layout mode `auto`, `compact`, `single-line`
- `--compact`: force compact layout
- `--single-line`: disable split-row compact layout

## Output Modes
- `table`: human-focused terminal table (default)
- `csv`: one header row plus one sample row per interval
- `json`: one JSON object per sample line

In `csv` and `json`, each rate metric includes a source tag field so you can see whether data came from NT counters, PDH fallback, or is unavailable.

Source tag fields:
- `src_flt`
- `src_pi`
- `src_po`
- `src_in`
- `src_sy`
- `src_cs`

Possible values:
- `NT`
- `PDH`
- `N/A`

## Field Reference
- `run`: running process snapshot count
- `blk`: blocked process count (not exposed directly on Windows, shown as `0`)
- `thr`: total thread count
- `swpd`: committed virtual memory
- `free`: available physical memory
- `buff`: non-paged pool memory
- `cache`: paged/system cache memory
- `flt`: page faults per second
- `pi`: pages input per second
- `po`: pages output per second
- `in`: processor interrupts per second
- `sy`: system calls per second
- `cs`: context switches per second
- `us`: user CPU percentage
- `sy` (cpu): system/kernel CPU percentage
- `id`: idle CPU percentage

## Notes
- In table mode, `vmstat` collects a baseline for one interval before showing the first sample.
- If NT counters are not available on your Windows build, `vmstat` falls back to PDH for live rates.
- If both NT and PDH sources are unavailable for a metric, that field is shown as `N/A`.

## Examples
- `vmstat 1 5`
- `vmstat -H 2`
- `vmstat --csv 1 5`
- `vmstat --json 1 5`
- `vmstat --layout compact 1 5`
- `vmstat --single-line 1 5`
- `vmstat -s`
