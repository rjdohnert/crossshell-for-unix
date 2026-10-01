# newshell

## What it does

Launches a new terminal session in Windows Terminal when available and falls back to the classic Console Host when needed.

It can open a fresh shell in the current directory or a chosen directory, optionally elevate to Administrator mode, select a Terminal profile, and run a command immediately inside the new terminal.

## Usage

```text
newshell.exe [OPTIONS] [-- <command> [args...]]
```

## Options

- `-a`, `--admin`: prompt for UAC elevation and open an Administrator terminal
- `-d`, `--dir <PATH>`: set the initial working directory for the new session
- `-p`, `--profile <NAME>`: specify a Windows Terminal profile name
- `-f`, `--force-conhost`: force `conhost.exe` instead of `wt.exe`
- `-h`, `--help`: display usage and exit
- `-v`, `--version`: display program version and exit

## Behavior

- If Windows Terminal is installed and reachable in `PATH`, `newshell` launches `wt.exe`.
- If Windows Terminal is unavailable, it falls back to `conhost.exe`.
- When a `--` separator is used, the remaining arguments are passed through to the shell command to run in the target session.
- If no directory is provided, the current working directory is used.
- If an elevation request is canceled by the user, the program reports the cancellation and exits with an error code.

## Examples

```text
newshell.exe
newshell.exe --admin
newshell.exe --dir "C:\Projects" --admin
newshell.exe --force-conhost --admin
newshell.exe -- ping 1.1.1.1 -t
```

## Notes

- The command uses modern Windows Terminal when available for richer tabbed terminal behavior.
- The fallback path preserves a classic console environment while still supporting elevated launch flows.
- The command is designed for interactive terminal launching rather than direct command execution in the current process.

## UNIX origin

This utility is a Windows-native convenience launcher for opening a new terminal session, inspired by the general shell pattern of spawning a fresh interactive command environment.
