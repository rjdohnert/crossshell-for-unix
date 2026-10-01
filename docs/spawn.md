# spawn

## What it does

Creates a Windows subprocess in a new console. With no command string, `spawn`
starts an interactive `cmd.exe` subshell. With a command string, it runs that
command through `cmd.exe /C`.

## Usage

`spawn [/qualifiers] [command-string]`

## Options

- `/HELP`, `/?`, `-h`, `--help`: display help text.
- `/WAIT`: wait for the subprocess to exit. This is the default.
- `/NOWAIT`: return immediately after starting the subprocess.
- `/NOTIFY`: print an informational message when used with `/NOWAIT`.
- `/PROCESS_NAME=name`, `/PROCESS=name`: set the new console window title.
- `/INPUT=filespec`, `/IN=filespec`: redirect standard input from a file.
- `/OUTPUT=filespec`, `/OUT=filespec`: redirect standard output and standard error to a file.

## Behavior Notes

- A successful launch prints the Windows process ID assigned by `CreateProcessW`.
- `/WAIT` returns the subprocess exit status after it exits.
- `/NOWAIT` returns after the subprocess starts; the parent does not wait for
  completion.
- Unrecognized slash qualifiers produce a warning and are ignored.

## Examples

Start an interactive subshell:

```text
spawn
```

Run a command and wait for it to finish:

```text
spawn "dir C:\\Windows"
```

Start a background process and capture its output:

```text
spawn /NOWAIT /NOTIFY /OUTPUT=backup.log "robocopy C:\\Src D:\\Bak /MIR"
```

## Exit Status

- `0`: help displayed, an asynchronous subprocess was started, or a waited
  subprocess exited successfully.
- Non-zero: subprocess exit status, process creation failure, or input/output
  file setup failure.

## UNIX origin

Adapted from the VAX/VMS `SPAWN` command also a BSD and Linux variants exist on those systems. On Windows it uses `CreateProcessW`
to start the subprocess.