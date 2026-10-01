# threads

## What it does

Reports the number of threads owned by each running Windows process. The utility takes a process snapshot and supports filtering, sorting, limiting, and structured output.

## Usage

```text
threads [-f FORMAT] [-s COLUMN] [--asc|--desc] [-n NAME] [-p PID] [-m COUNT] [-t COUNT]
```

## Options

- `-f FORMAT`, `--format FORMAT`: select `table`, `csv`, `json`, or `pipe` output. The default is `table`.
- `-s COLUMN`, `--sort COLUMN`: sort by `threads`, `pid`, or `name`. The default is `threads`.
- `--asc`: sort in ascending order.
- `--desc`: sort in descending order. This is the default.
- `-n NAME`, `--name NAME`: filter by a case-insensitive process-name substring.
- `-p PID`, `--pid PID`: filter by exact process ID.
- `-m COUNT`, `--min-threads COUNT`: exclude processes with fewer than `COUNT` threads.
- `-t COUNT`, `--top COUNT`: return only the first `COUNT` records after filtering and sorting. By default, all records are returned.
- `-h`, `--help`, `/?`: display usage information.

## Output

The default table reports the process ID, thread count, and executable name.

Structured formats use these fields:

- CSV: `PID`, `ThreadCount`, `ProcessName`
- JSON: `pid`, `threadCount`, `processName`
- Pipe-delimited: `PID|ThreadCount|ProcessName`

## Examples

```powershell
threads
threads -p 7992
threads -t 5
threads -n Code -s name --asc
threads -f json -n chrome
threads -f csv -m 50 -s threads
threads -f pipe | findstr /V "explorer"
```

## Exit status

- `0`: output completed or help was displayed.

## Notes

This is a point-in-time process snapshot, not a continuously refreshing monitor. Thread counts come from the operating system's process snapshot and may change immediately after collection.

## Windows origin

A Windows-native process diagnostic utility inspired by Task Manager and Unix process-monitoring tools.
