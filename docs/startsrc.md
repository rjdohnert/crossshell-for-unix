# startsrc

## What it does
Starts a Windows service using AIX-compatible startsrc command style.

## Usage
startsrc -s SERVICE [-t SECONDS]

## Options
- -s SERVICE: service name to start.
- -t SECONDS: wait timeout in seconds. Default: 30.
- -h, --help: display help and exit.
- -v, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error (service control failure)
- 2: invalid command syntax

## Examples
```powershell
startsrc -s Spooler
startsrc -s W32Time -t 60
```

## Windows Mapping Notes
- Wraps StartService + QueryServiceStatusEx wait loop.
- Emits AIX-like status messages for start operations.
