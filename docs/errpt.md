# errpt

## What it does
Displays Windows Event Log records with an AIX-style summary view.

## Usage
errpt [options]

## Options
- -l, --log NAME: event log to read (system, application, security). Default: system.
- -n, --lines COUNT: max number of entries to print. Default: 50.
- -a, --all: print all available entries.
- --output MODE: output as table, json, or csv (default: table).
- --table: shorthand for --output table.
- --json: emit a JSON array to standard output.
- --csv: emit CSV with a header row to standard output.
- -h, --help: display help and exit.
- -v, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error (log open/read failure)
- 2: invalid command syntax

## Examples
```powershell
errpt
errpt -l application -n 100
errpt --log security --all
errpt --json | jq '.[].summary'
errpt --csv > system-events.csv
errpt -l application --output table | findstr /i error
```

## Windows Mapping Notes
- Reads classic Windows Event Logs (System/Application/Security) through Event Log APIs.
- Table output uses timestamp, severity, event ID, source, and summary columns.
- JSON and CSV use timestamp, severity, event_id, source, and summary fields.
- Structured output is written to stdout without decorative text so it can be piped to jq, PowerShell, findstr, or other tools.
