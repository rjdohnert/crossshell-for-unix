# driverctl

## What it does
Enumerates installed Windows PnP devices and reports driver health, architecture, and stack details.

For each device it reports the name, setup class/category, driver type (kernel-mode, file-system filter, Win32 service, or UMDF host), driver provider and version, owning service, and any upper/lower filter drivers in the device stack. With `--test`, it queries the Configuration Manager for problem codes (Code 10, Code 43, disabled devices, etc.) and assigns a health verdict.

A uniquely named, human-readable diagnostic log is created in `~/driverctl-logs/` when running the default report. Existing logs are never overwritten. Structured output formats do not create a log.

## Options
- `-h`, `--help`: show help text
- `--list`: enumerate all installed devices, categories, and drivers
- `--test`: run live hardware and driver malfunction diagnostics
- `--category <name>`: filter devices by setup class/category (e.g., Display, Net, USB)
- `--errors-only`: suppress healthy devices; requires `--test`
- `--table`: emit results as an aligned table on stdout
- `--json`: emit results as a JSON array on stdout (for jq / pipelines)
- `--csv`: emit results as CSV on stdout (for spreadsheets / parsers)

CSV fields that could be interpreted as spreadsheet formulas are prefixed with an apostrophe to keep exported device data literal.

## Examples
- `driverctl --test --category Display`
- `driverctl --test --errors-only`
- `driverctl --list --json | jq ".[] | .deviceName"`
- `driverctl --list --csv --category Net`

## Pipeline integration
Structured formats (`--table`/`--json`/`--csv`) write clean data to stdout only, so output can be piped to `jq`, `findstr`, `Select-String`, or a CSV importer. When stdin is piped, driverctl reads the first token as an additional category filter.

## Notes
Running without administrator rights may leave some device status and problem codes incomplete; the report notes this and suggests re-running elevated.
Without `--test`, the human-readable summary reports that device health was not tested rather than treating inventory records as healthy.

Unknown options and missing `--category` values are rejected. A device-enumeration error is reported to stderr and returns a non-zero exit status instead of being presented as an empty or partial scan.

## UNIX origin
Not a UNIX utility; a Windows-native diagnostic tool in the spirit of AIX `lsdev`/Linux `lsmod`+`lspci` driver inspection.
