# handles

## What it does

Reports the number of open handles owned by each running Windows process. The utility takes a process snapshot, queries each accessible process for its handle count, and supports filtering, sorting, limiting, and structured output.

## Usage

```text
handles [-f FORMAT] [-s COLUMN] [--asc|--desc] [-n NAME] [-p PID] [-t COUNT]
```

## Options

- `-f FORMAT`, `--format FORMAT`: select `table`, `csv`, `json`, or `pipe` output. The default is `table`.
- `-s COLUMN`, `--sort COLUMN`: sort by `handles`, `pid`, or `name`. The default is `handles`.
- `--asc`: sort in ascending order.
- `--desc`: sort in descending order. This is the default.
- `-n NAME`, `--name NAME`: filter by a case-insensitive process-name substring.
- `-p PID`, `--pid PID`: filter by exact process ID.
- `-t COUNT`, `--top COUNT`: return only the first `COUNT` records after filtering and sorting. By default, all records are returned.
- `-h`, `--help`, `/?`: display usage information.

## Output

The default table reports the process ID, handle count, and executable name. If a process cannot be queried, its handle-count field is shown as `<AccessDenied>`.

Structured formats use these fields:

- CSV: `PID`, `HandleCount`, `ProcessName`, `AccessDenied`
- JSON: `pid`, `handleCount`, `processName`, `accessDenied`
- Pipe-delimited: `PID|HandleCount|ProcessName`

Pipe-delimited output represents an unavailable handle count as `0`; CSV and JSON retain the numeric value and expose access status separately.

## Examples

```powershell
handles
handles -p 7992
handles -n Code -s name --asc
handles -f json -t 5
handles -n chrome -f csv
handles -f pipe | findstr /V "0"
```

## Exit status

- `0`: output completed or help was displayed.

## Notes

The utility uses `PROCESS_QUERY_LIMITED_INFORMATION`, so it can run without administrator privileges. Protected or otherwise inaccessible processes remain in the results and are marked as access denied.

## Windows origin

A Windows-native process diagnostic utility inspired by Unix-style inspection tools.
