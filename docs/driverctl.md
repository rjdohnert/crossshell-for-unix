# driverctl

## What it does
Enumerates installed Windows PnP devices and reports driver health, architecture, and stack details.

For each device it reports the name, setup class/category, driver type (kernel-mode, file-system filter, Win32 service, or UMDF host), driver provider and version, owning service, and any upper/lower filter drivers in the device stack. With `--test`, it queries the Configuration Manager for problem codes (Code 10, Code 43, disabled devices, etc.) and assigns a health verdict.

A timestamped, human-readable diagnostic log is written to `~/driverctl-logs/` when running the default report.

## Options
- `-h`, `--help`: show help text
- `--list`: enumerate all installed devices, categories, and drivers
- `--test`: run live hardware and driver malfunction diagnostics
- `--category <name>`: filter devices by setup class/category (e.g., Display, Net, USB)
- `--errors-only`: suppress healthy devices; only report warnings and failures
- `--table`: emit results as an aligned table on stdout
- `--json`: emit results as a JSON array on stdout (for jq / pipelines)
- `--csv`: emit results as CSV on stdout (for spreadsheets / parsers)

## Examples
- `driverctl --test --category Display`
- `driverctl --test --errors-only`
- `driverctl --list --json | jq ".[] | .deviceName"`
- `driverctl --list --csv --category Net`

## Pipeline integration
Structured formats (`--table`/`--json`/`--csv`) write clean data to stdout only, so output can be piped to `jq`, `findstr`, `Select-String`, or a CSV importer. When stdin is piped, driverctl reads the first token as an additional category filter.

## Notes
Running without administrator rights may leave some device status and problem codes incomplete; the report notes this and suggests re-running elevated.

## UNIX origin
Not a UNIX utility; a Windows-native diagnostic tool in the spirit of AIX `lsdev`/Linux `lsmod`+`lspci` driver inspection.
