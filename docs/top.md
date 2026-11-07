# top

## What it does
Displays a live, continuously refreshed view of processes and system CPU, physical memory, and commit usage. It requires an interactive Windows console.

## Options
- `-d`, `--delay <seconds>`: set refresh delay to a positive whole number of seconds (default `1`)
- `-i <seconds>`: alias for `-d`
- `-n <rows>`: number of process rows to display (default `20`)
- `-s <mode>`: sort mode: `cpu`, `mem`, `pid`, `name`, or `time` (default `cpu`)
- `-u <username>`: show processes owned by the specified username
- `-h`, `--help`: show command help
- `-V`, `--version`: show version

## Interactive keys
- `q`, `Esc`, `Ctrl+X`: quit
- `h`, `?`: show interactive help
- `d`: change refresh delay
- `n`: change the number of process rows
- `u`: filter by username (enter a blank line to clear the filter)
- `c`, `m`, `p`, `a`, `t`: sort by CPU, memory, PID, command name, or CPU time
- `k`: terminate a process by PID

The `ACTIVITY` column is a sample-based indicator: `active` means the process used more than 0.5% CPU during the last sample; `idle` means it did not. It does not report the Windows scheduler's process state.

The memory summary reports physical memory and Windows commit usage. Commit availability is not reported as free swap.
