# mkfifo

## What it does
Creates FIFO special files as Windows named pipes in the NPFS namespace. This is a Windows-native implementation, not a POSIX emulation layer.

## Supported options
- `-m sddl`: set the Windows security descriptor in SDDL format
- `-t byte|message`: select byte-stream or message-mode pipe semantics
- `-d duplex|in|out`: set the pipe direction
- `-p`, `--persist`: keep the pipe open until Ctrl+C is pressed
- `-v`, `--verbose`: print the underlying Win32 pipe attributes
- `-h`, `--help`: show help text

## Behavior
- Pipe names are normalized to the Windows namespace form `\\.\pipe\name` when needed.
- The default security descriptor is `D:(A;;GRGW;;;WD)` (Everyone Read/Write).
- When `-p` is used, the command keeps the server handle open and waits for Ctrl+C to close it.
- On Windows, named pipes exist in NPFS and are not created as regular filesystem FIFO files.

## Examples

```text
mkfifo my_pipe
mkfifo -p -v app_fifo
mkfifo -t message -m "D:(A;;GA;;;BA)" secure_fifo
mkfifo -d in -p logger_fifo
```

## UNIX origin
A standard Unix file-creation utility from early BSD and System V releases, adapted here for Windows named-pipe behavior.
