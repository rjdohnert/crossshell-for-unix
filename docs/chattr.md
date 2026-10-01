# chattr

## What it does
Provides a baseline `chattr` command that maps a small set of Unix-style attribute flags onto Windows file attributes.

## Options
- `+a`, `-a`: set or clear the archive flag
- `+h`, `-h`: set or clear the hidden flag
- `+s`, `-s`: set or clear the system flag
- `+i`, `-i`: set or clear the immutable shim, backed by the read-only attribute
- `-R`: recurse into directories
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## UNIX origin
A Linux file-attribute utility commonly used to protect files or toggle metadata flags.