# sed

## What it does
Edits text streams using simple scripting commands.

## Options
- `-e script`: add a script to the command list
- `-f script-file`: read the script from a file
- `-n`: suppress automatic printing
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Compatibility Notes
- This implementation currently supports `s`, `d`, and `p` commands with optional line or regex addresses.
- In-place editing (`-i`) is not currently implemented.

## UNIX origin
A classic Unix text-processing utility from early BSD and System V releases.
