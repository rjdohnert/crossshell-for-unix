# findmnt

## What it does
Provides a baseline `findmnt` command that lists mounted Windows volumes, their targets, filesystem types, and simple read/write status.

## Options
- `-n`, `--noheadings`: suppress the header row
- `-h`, `--help`: show help
- `-V`, `--version`: show version information

## Notes
- This is a Windows-native approximation of `findmnt` built from logical drive and volume APIs.
- Network mappings are shown using the mapped UNC share when available.

## UNIX origin
`findmnt` is used to inspect mounted filesystems and mount relationships.