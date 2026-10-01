# script

## What it does
Provides a baseline `script` command that records a command session transcript to a file while teeing output to the console.

## Options
- `-a`, `--append`: append to the output file instead of truncating it
- `-h`, `--help`: show help
- `-V`, `--version`: show version information

## Notes
- If no command is supplied, `script` launches `cmd.exe`.
- This baseline records command output rather than emulating a full POSIX tty transcript.

## UNIX origin
`script` records terminal sessions for later review.