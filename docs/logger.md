# logger

## What it does
Provides a baseline `logger` command that writes messages to the console and attempts to send them to the Windows Application Event Log.

## Options
- `-t TAG`, `--tag TAG`: set the tag prefix for the message
- `-h`, `--help`: show help
- `-V`, `--version`: show version information

## Notes
- If no message arguments are provided, `logger` reads messages from standard input line by line.
- Event Log writes are best-effort; console output remains available even when registration fails.

## UNIX origin
`logger` sends messages to the system log.