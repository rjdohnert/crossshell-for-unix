# sync

## What it does
Forces pending file or volume writes from Windows caches to the underlying storage. With no path operands, it attempts to synchronize all active fixed and removable volumes.

## Usage
`sync [-dfhqrsvV] [file ...]`

## Options
- `-d`, `--data`: synchronize file data without metadata where supported
- `-f`, `--file-system`: synchronize the filesystem containing each specified path
- `-r`, `--removable`: restrict synchronization to removable media
- `-v`, `--verbose`: show each target and synchronization result
- `-q`, `--quiet`: suppress informational output and warnings
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Examples
```text
sync
sync report.dat
sync --file-system C:\data
sync --removable --verbose
```

## Notes
Flushing complete volumes generally requires an elevated console. The command returns non-zero if any requested target cannot be synchronized.

## UNIX origin
Modeled after the Unix `sync` utility, using Windows cache and volume APIs.