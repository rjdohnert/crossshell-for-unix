# lsbt

## What it does
Lists local Bluetooth radios and paired or connected Bluetooth devices, including device name, address, manufacturer, class-of-device, and connection state.

## Supported options
- `-u`, `--in-use-only`: show only connected devices
- `-p`, `--paired-only`: show only remembered/paired devices
- `-s`, `--scan`: trigger an active Bluetooth inquiry scan
- `-t`, `--type <TYPE>`: filter by device type/class
- `-m`, `--mfg <NAME>`: filter by manufacturer or vendor name
- `-n`, `--name <NAME>`: filter by device name substring
- `-a`, `--addr <MAC>`: filter by Bluetooth MAC address
- `-r`, `--radios-only`: display only local Bluetooth host radios
- `-f`, `--format <table|json|csv>`: choose output format
- `--sort <CRITERIA>`: sort by name, mfg, type, status, or lastused
- `--no-color`: disable ANSI output coloring
- `-h`, `--help`, `/?`: display help text

## Notes
- Bluetooth enumeration uses the Windows Bluetooth APIs and SetupAPI registry lookups.
- Device class decoding is reported as a friendly device type such as Audio/Video, HID, Phone, or Computer.
- The tool can distinguish between connected, paired, and remembered devices.

## Examples

```text
lsbt
lsbt -u
lsbt -s --format json
lsbt -m Sony
lsbt --type Audio --in-use-only
lsbt --radios-only
```

## UNIX origin
A compatibility-style utility rather than a core command from a single historic Unix release.
