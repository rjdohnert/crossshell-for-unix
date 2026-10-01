# lssrc

## What it does
Lists Windows services in an AIX SRC-style table.

## Usage
lssrc [options]

## Options
- -a: include stopped services (default output is active/running services only).
- -s NAME: show only one service by service name.
- -h, --help: display help and exit.
- -v, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error (SCM/service failure)
- 2: invalid command syntax

## Examples
```powershell
lssrc
lssrc -a
lssrc -s Spooler
```

## Windows Mapping Notes
- Uses Service Control Manager enumeration APIs.
- Prints subsystem, process ID, and status in an AIX-inspired format.
