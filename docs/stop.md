# stop

## What it does
Stops one or more processes by PID, with a signal-selection mode similar to the classic Unix `kill` command.

## Usage
```text
stop [OPTIONS] PID...
stop -s SIGNAL PID...
stop -l
```

## Options
- `-s`, `--signal SIGNAL`: specify the signal to send.
- `-l`, `--list`: list the supported signals.
- `-h`, `--help`: show help text and exit.
- `--version`: show version information and exit.

## Supported Signals
- `1` / `HUP`
- `2` / `INT`
- `3` / `QUIT`
- `9` / `stop`
- `15` / `TERM`

## Signal behavior
- `TERM` performs a graceful shutdown attempt using `WM_CLOSE`, then falls back to `TerminateProcess` if no visible windows are found.
- `INT` and `QUIT` generate console control events when the target process owns a console.
- `stop` force-terminates the process immediately.

## Exit Codes
- `0`: all targets handled successfully.
- `1`: one or more targets failed or invalid input was supplied.

## Examples
```text
stop 1234
stop -s TERM 1234 5678
stop -l
```

## Windows Mapping Notes
- This is a Windows-native process control utility that models common Unix signal semantics.
- It uses the Windows process handle API, console control events, and window-message shutdown attempts to approximate signal behavior on a native process model.
