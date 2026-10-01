# whereami

## What it does
Reports the host's geographic coordinates using Windows location sensors or network positioning. It can also display altitude, accuracy, civic address, and network telemetry.

## Usage
`whereami [-aAdhjnqsvV] [-f format] [-t seconds] [--from=provider] [--basedir=directory] [--statedir=directory]`

## Options
- `-f format`, `--format=format`: select `raw`, `sexagesimal`, `json`, `human`, or `address` output
- `-j`, `--json`: use JSON output
- `-s`, `--sexagesimal`: use degrees, minutes, and seconds
- `-a`, `--accuracy`: include horizontal accuracy in meters
- `-A`, `--altitude`: include altitude when available
- `-d`, `--address`: display a civic address
- `-n`, `--network`: include public IP, local IP, ISP, and hostname data
- `-t seconds`, `--timeout=seconds`: set the provider timeout; the default is 5 seconds
- `--from=provider`: select `auto`, `sensor`, or `network`
- `-v`, `--verbose`, `--debug`: show diagnostic details
- `-q`, `--quiet`: suppress diagnostics and warnings
- `--basedir=directory`: set the configuration base directory
- `--statedir=directory`: set the location-record state directory
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Examples
```text
whereami
whereami --json --network
whereami --sexagesimal --accuracy
whereami --from=network --format=address
```

## Notes
Sensor results depend on Windows location permissions. Network positioning requires network access and may be less precise.