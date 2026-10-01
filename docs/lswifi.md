# lswifi

## What it does
Lists Wi‑Fi adapters and nearby access points, including connection state, SSID, BSSID, signal strength, band, channel, and basic quality information.

## Supported options
- `-h`, `--help`: show help text
- `-f`, `--format <table|json|csv>`: select output format
- `-s`, `--scan`: trigger a fresh WLAN scan before reading access points
- `-a`, `--adapters-only`: show adapter information only
- `-n`, `--networks-only`: show visible access points only
- `--min-signal <0-100>`: hide networks weaker than the selected quality threshold
- `--ssid <FILTER>`: only display access points whose SSID contains the provided text
- `--band <2.4|5|6>`: filter by radio band name or prefix
- `--no-color`: disable ANSI terminal colors

## Notes
- The implementation uses the native Windows WLAN API and is intended for local diagnostics on Wi‑Fi-capable systems.
- The tool can report both the currently connected SSID/BSSID and the visible neighboring access points from the last scan.
- Output defaults to a readable table in a terminal, with JSON and CSV output available for scripting.

## Examples

```text
lswifi
lswifi -s
lswifi --format json
lswifi --min-signal 60 --band 5
lswifi --format csv > wifi_scan.csv
```

## UNIX origin
This is a Windows-native diagnostic utility modeled on Unix-style network and hardware reporting tools, rather than a direct classic Unix command.
