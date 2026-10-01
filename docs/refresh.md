# refresh

## What it does
Sends a service refresh/reload control to a Windows service, with optional restart fallback.

## Usage
refresh -s SERVICE [options]

## Options
- -s SERVICE: service name to refresh.
- -r, --restart: if refresh control is unsupported, stop and start the service.
- -t SECONDS: wait timeout in seconds for fallback restart workflow. Default: 30.
- -h, --help: display help and exit.
- -v, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error (service control failure)
- 2: invalid command syntax

## Examples
```powershell
refresh -s W32Time
refresh -s W32Time --restart
```

## Windows Mapping Notes
- Primary action uses SERVICE_CONTROL_PARAMCHANGE.
- Restart fallback approximates AIX refresh semantics for services without reload support.
