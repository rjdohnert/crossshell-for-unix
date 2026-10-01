# stat

## What it does
Displays detailed file and directory status information for one or more paths.

It reports metadata such as type, absolute path, size, timestamps, attributes, owner, and link/junction target details when applicable.

## Options
- `-h`, `--help`: show help text
- `-v`, `--version`: show version and copyright information

## Examples
- `stat C:\\Windows\\System32\\cmd.exe`
- `stat C:\\Users\\Public\\Documents`
- `stat .\\mysymlink.lnk`

## UNIX origin
A standard Unix file-properties utility from early BSD and System V systems.
Used today in IBM AIX and OpenBSD.