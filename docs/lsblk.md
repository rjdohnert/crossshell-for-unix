# lsblk

## What it does
Provides a baseline `lsblk` command that lists logical block-style devices, mount points, filesystem types, and sizes on Windows.

## Options
- `-n`, `--noheadings`: suppress the header row
- `-h`, `--help`: show help
- `-V`, `--version`: show version information

## Notes
- This is a Windows-native approximation based on logical drive enumeration.
- Output is focused on practical inventory rather than exact Linux block-device topology.

## UNIX origin
`lsblk` lists block devices and mount points.