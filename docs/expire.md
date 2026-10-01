# expire

Run a command with a maximum allowed runtime.

## Synopsis

expire [OPTION]... DURATION COMMAND [ARG]...
expire --help
expire --version

## Description

`expire` starts COMMAND and terminates it if still running after DURATION.

Mandatory arguments to long options are mandatory for short options too.

## Options

- `-s`, `--signal=SIGNAL`: signal to send on timeout
  - Supported names: `TERM`, `KILL`, `INT`, `HUP`, `QUIT`
  - Supported numbers: `1`, `2`, `3`, `9`, `15`
  - Default: `TERM`
- `-k`, `--kill-after=DURATION`: send SIGKILL (9) if COMMAND is still running this long after the initial timeout signal
- `--preserve-status`: exit with the same status as COMMAND, even when it times out
- `-f`, `--foreground`: allow COMMAND to attach to the foreground console session and read directly from standard TTY input
- `-v`, `--verbose`: print diagnostics to standard error for timeout signal handling
- `--help`: display help and exit
- `--version`: display version and exit

## Duration Specs

DURATION is a floating-point number with an optional suffix:

- `s`: seconds (default)
- `m`: minutes
- `h`: hours
- `d`: days

A duration of `0` disables the associated timeout (infinite wait).

## Windows Signal Mapping

Windows does not use POSIX signals natively, so signals are emulated:

- `TERM` / `15`: graceful WM_CLOSE or CTRL_BREAK plus Job Object tree termination
- `KILL` / `9`: immediate force termination (TerminateJobObject)
- `INT` / `2`: Win32 CTRL_C_EVENT
- `QUIT` / `3`: Win32 CTRL_BREAK_EVENT
- `HUP` / `1`: graceful console close behavior

## Exit Status

- `124`: command timed out and `--preserve-status` was not set
- `125`: internal error or invalid CLI arguments
- `126`: command was found but could not be invoked
- `127`: command could not be found
- `137`: command was force-killed by SIGKILL (`128 + 9`)
- Otherwise: exit status of COMMAND

## Examples

Run ping for 5 seconds:

```powershell
expire 5s ping -t 127.0.0.1
```

Send SIGKILL on timeout after 1.5 minutes:

```powershell
expire -s KILL 1.5m my_app.exe --process
```

Send SIGTERM at 30 seconds, then SIGKILL 10 seconds later if still running:

```powershell
expire -k 10s 30s long_running_job.exe
```

Preserve child status even if timeout occurs:

```powershell
expire --preserve-status 10 python script.py
```

## UNIX Origin

GNU coreutils `timeout` inspired command behavior with Windows-specific process and signal emulation.
