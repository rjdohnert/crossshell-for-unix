# printf

## What it does
Formats and prints text and data to standard output.

## Options
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Builtin notes (ksh)
- `printf [-v varname] format [arguments ...]`
- `%q` emits shell-quoted output.
- `%T` emits local date/time as `YYYY-MM-DD HH:MM:SS`.
	- No argument or `-1`: current local time.
	- Numeric argument: epoch seconds.

## Examples
- `printf '%s\n' hello`
- `printf '%q\n' "a b c"`
- `printf '%T\n'`
- `printf '%T\n' 0`

## UNIX origin
A standard Unix shell builtin and utility from early POSIX-compatible systems.
