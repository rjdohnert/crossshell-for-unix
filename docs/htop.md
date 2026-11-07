# htop

## What it does

Displays a full-screen interactive process monitor for Windows with per-core CPU usage, physical memory and system commit meters, tree view, search, filtering, tagging, setup controls, and process actions.

## Command-line options

- `-d`, `--delay DELAY`: Set the refresh interval in tenths of a second. The default is `15` (1.5 seconds); values must be positive integers.
- `-u`, `--user USERNAME`: Show only processes owned by the specified username.
- `-p`, `--pid PID...`: Show only the specified process IDs. Multiple IDs can follow one option, and the option can be repeated.
- `-h`, `--help`: Show command-line help.
- `-V`, `--version`: Show the version.

## Interactive keys

- `F1` / `h`: help
- `F2` / `S`: setup
- `F3` / `/`: search
- `F4` / `\\`: filter
- `F5` / `t`: toggle tree view
- `F6`: change sort field
- `F7` / `[`: lower priority
- `F8` / `]`: raise priority
- `F9` / `k`: open process actions
- `F10` / `q`: quit
- `Space`: tag or untag the selected process
- `U`: clear all tags
- `u`: user filter
- Arrow keys, Page Up, Page Down, Home, End: navigate the table
- Mouse click and wheel: select and scroll

The process-action menu can terminate, suspend, or resume the selected process. If any processes are tagged, actions apply to all tagged processes, including rows hidden by filters; actions affecting multiple tagged processes require confirmation. Termination is immediate and does not provide a graceful shutdown request. Results are reported in the status line.

When more than 8 logical processors are present, core percentages are shown in a compact multi-row layout so no cores are omitted. Commit usage is shown as system commit total/limit; it is not a pagefile-only measurement.

## Notes

- The Windows-native C++17 implementation is split into CLI, app, telemetry,
  console, formatting, and shared type/API modules.
- Process memory columns use the process working set; `SHR` is the working set
  minus the process's private working-set size.
- Telemetry and process suspend/resume use Windows NT APIs and may vary across
  Windows versions.
- The interactive display requires a console at least 80 columns by 20 rows.

## UNIX origin

This is an htop-style process monitor adapted for Windows rather than a direct portable clone of a specific Unix release in this repository.
