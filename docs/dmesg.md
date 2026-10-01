# dmesg

## Overview

dmesg reads Windows System event log records and prints kernel, driver, and service-control messages in a compact dmesg-like format.

It supports provider-group filtering, severity filtering, provider-name substring matching, scan depth controls, and optional ANSI color output.

## Synopsis

```text
dmesg [options]
```

## Options

- -h, --help: show comprehensive help
- -V, --version: show version
- -n, --max-events COUNT: max matching events to print, default 50
- -m, --max-scan COUNT: max records to scan, default 1000
- -k, --kernel: include Microsoft-Windows-Kernel* providers
- -K, --no-kernel: exclude Microsoft-Windows-Kernel* providers
- -d, --driver: include Microsoft-Windows-Driver* providers
- -D, --no-driver: exclude Microsoft-Windows-Driver* providers
- -s, --scm: include Service Control Manager providers
- -S, --no-scm: exclude Service Control Manager providers
- -p, --provider-contains TEXT: case-insensitive provider substring filter
- -l, --min-level LEVEL: minimum severity threshold
- -c, --color: enable ANSI colors
- -C, --no-color: disable ANSI colors

Accepted LEVEL values:

- critical or 1
- error or 2
- warning or warn or 3
- info or information or 4
- verbose or debug or 5

## Output format

Each event prints as:

- timestamp
- level label
- provider name
- event id
- key data fields (when present)

Default provider classes include kernel, driver, and SCM events.

## Examples

```text
dmesg
dmesg -n 20
dmesg -k -S -l warning
dmesg -p disk -m 5000
dmesg -C
```

## Notes

- Query source is the Windows System channel.
- If no provider groups remain enabled, argument parsing fails with guidance.
- Color output is enabled only when the terminal supports virtual terminal processing.
