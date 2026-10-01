# nohup

## Overview

nohup runs a command detached from interactive console hangup events and redirects terminal-attached streams to durable outputs.

This Windows implementation handles console close and control events, supports robust command argument quoting, and follows standard nohup-style exit semantics.

## Synopsis

```text
nohup COMMAND [ARGUMENT...]
nohup OPTION
```

## Description

- Ignores Ctrl+C, Ctrl+Break, console close, logoff, and shutdown events for the launched process group.
- If stdin is attached to a console, it is redirected from NUL.
- If stdout is attached to a console, output is appended to nohup.out.
- If stderr is attached to a console, stderr is redirected to stdout.

Output file selection order:

1. .\nohup.out
2. %USERPROFILE%\nohup.out
3. %TEMP%\nohup.out

## Options

- -h, --help, /?: show help text
- -v, --version: show version info

## Process launch behavior

- Uses DETACHED_PROCESS and CREATE_NEW_PROCESS_GROUP.
- For batch files (.bat, .cmd), wraps execution through cmd.exe /c.
- If direct execution fails for non-batch commands, retries through cmd.exe /c.

## Exit status

- 0: help or version output success
- 126: command found but could not be invoked or permission/elevation failure
- 127: command not found, missing operand, or internal launch failure
- other: child process exit status when execution succeeded

## Examples

```text
nohup my_script.bat
nohup python long_job.py "arg with spaces" 100
nohup ping 127.0.0.1 -t > custom_log.txt 2>&1
```

## Notes

- The tool prints the selected nohup.out path when it redirects stdout.
- Existing redirections in the command line still apply.
