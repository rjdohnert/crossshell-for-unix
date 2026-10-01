# prtconf

## What it does
Prints system configuration summary with AIX-inspired headings.

## Usage
prtconf [options]

## Options
- -v: include device class summary counts.
- -h, --help: display help and exit.
- -V, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error
- 2: invalid command syntax

## Examples
```powershell
prtconf
prtconf -v
```

## Windows Mapping Notes
- Reports node name, architecture, CPU count, memory size, OS level, and total present devices.
- Verbose mode adds per-class device counts from SetupAPI enumeration.
