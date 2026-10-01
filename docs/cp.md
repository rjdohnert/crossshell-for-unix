# cp

## What it does
Copies files and directory subtrees, with Windows-aware attribute preservation and interactive overwrite handling.

## Supported options
- `-f`: force overwrite of an existing destination without prompting
- `-i`: prompt before overwriting an existing destination
- `-p`: preserve timestamps and file attributes where supported
- `-e`: preserve Windows extended attributes and ACL-related metadata
- `-P`: do not dereference symlinks
- `-L`: dereference symlinks when copying
- `-r`, `-R`: recurse into directories
- `-v`, `--verbose`: enable verbose output
- `--no-progress`: disable the copy progress bar
- `--json`: emit one line-delimited JSON object per successful copy, including source, destination, and byte count
- `-h`, `--help`: show help text

## Notes
- This implementation is tuned for HP-UX compatibility on Windows.
- Progress output is enabled by default for interactive terminal usage.
- Copying directory trees is recursive only when `-r` or `-R` is supplied.
- In `--json` mode the tool writes JSON to stdout instead of the usual progress/text output, making it easy to pipe into PowerShell or jq.

## Examples

```text
cp kernel.sys C:\Backup\
cp -r /opt/app D:\Deployment\
cp -ip file1.dat file2.dat
cp -R -e -p ProjectDir E:\Archive\
cp --json source.txt destination.txt
```

## UNIX origin
A core Unix file-management utility from early BSD and System V releases.
