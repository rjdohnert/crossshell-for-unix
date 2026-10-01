# rcp

## What it does
Copies files and directories between a local Windows machine and a remote Unix or Linux host using the remote copy protocol.

This implementation is path-traversal hardened and supports UTF-8 / wide-path handling on Windows.

## Usage
```text
rcp [options] <source> <destination>
```

## Options
- `-r`, `--recursive`: recursively copy directory trees
- `-p`, `--preserve`: preserve timestamps during copy
- `-P <port>`: use a custom TCP port instead of the default
- `-h`, `-?`, `--help`: show help text

## Path Syntax
- Local path: `C:\path\to\file.txt`
- Local path with spaces: `"C:\path with spaces\file.txt"`
- Remote path: `[user@]host:/remote/path/to/file`

## Examples
- `rcp file.txt user@host:/tmp/file.txt`
- `rcp -r C:\Data user@host:/tmp/data`
- `rcp -p -P 514 user@host:/tmp/file.txt C:\Backups\file.txt`

## Notes
- The default remote port is `514`.
- The command can copy both files and directory trees.
- Temporary files are tracked and cleaned up if the process is interrupted.
- Remote targets rely on the classic `rcp`/`rshd` workflow.

## UNIX origin
A classic remote copy utility from BSD and other Unix systems.