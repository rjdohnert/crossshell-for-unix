# setfacl

## What it does
Provides a baseline `setfacl` command that applies a simple ACL entry to Windows files and directories.

## Options
- `-m`, `--modify`: apply the supplied ACL entry
- `-b`, `--remove`: clear the DACL as a baseline removal action
- `-R`, `--recursive`: recurse into directories
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## ACL syntax
- `u:NAME:rwx`: user entry
- `g:NAME:rwx`: group entry
- `o::rwx`: everyone entry

## UNIX origin
A common POSIX ACL management utility used in mixed-platform permission workflows.