# lsusb

## What it does
Lists USB devices attached to the system, including the device name, manufacturer, device type/class, and hardware ID.

## Supported options
- `--table`: print tabular output (default)
- `--csv`: print CSV output
- `--json`: print JSON output
- `-q`, `--quiet`: compact output mode
- `-`: read filters from standard input
- `-h`, `--help`, `/?`: display help text

## Notes
- USB enumeration is performed via the Windows SetupAPI device interfaces.
- Entries can be filtered by name, manufacturer, or hardware ID using one or more positional filter arguments.
- Output includes the device index, name, manufacturer, type, and hardware ID.

## Examples

```text
lsusb
lsusb --json
lsusb --csv
lsusb Logitech
lsusb "USB\VID_046D&PID_C52B"
```

## UNIX origin
A Unix/Linux hardware-reporting utility rather than a single classic Unix release.
