# last

## What it does
Shows recent logon and logoff activity derived from Windows event logs.

This implementation can read either the live Security and System event logs or an exported `.evtx` file, then filter the output to selected users.

## Usage
```text
last [options] [user ...]
```

## Options
- `-n number`: limit output to the specified number of lines
- `-f file`: read from an exported `.evtx` file instead of the live event logs
- `-h`, `--help`: show help text

## What it Reports
- Successful logons and logoffs
- Explicit logoffs
- System start and stop events
- Shutdown or reboot transitions when they appear in the log source

## Examples
- `last`
- `last -n 20`
- `last Administrator`
- `last -f C:\Logs\Security.evtx`

## Notes
- If no `.evtx` file is provided, the command queries the live Security and System event logs.
- Administrator privileges may be required to read the Security log.
- Multiple user names can be provided to filter the output.

## UNIX origin
A traditional Unix login-history utility from BSD and System V environments.