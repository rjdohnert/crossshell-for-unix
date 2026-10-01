# stopsrc

## What it does
Stops a Windows service using AIX-compatible stopsrc command style.

## Usage
stopsrc -s SERVICE [-t SECONDS]

## Options
- -s SERVICE: service name to stop.
- -t SECONDS: wait timeout in seconds. Default: 30.
- -h, --help: display help and exit.
- -v, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error (service control failure)
- 2: invalid command syntax

## Examples
```powershell
stopsrc -s Spooler
stopsrc -s W32Time -t 45
```

## Windows Mapping Notes
- Wraps SERVICE_CONTROL_STOP + QueryServiceStatusEx wait loop.
- Emits AIX-like status messages for stop operations.
