# ls

## What it does
Lists files and directories, with Windows-aware attribute reporting and optional structured output.

## Supported options
- `-a`: include hidden and dot entries such as `.` and `..`
- `-A`: include almost-all entries except `.` and `..`
- `-l`: long format, including Windows attribute mask, owner/domain, size, and timestamp
- `-F`: append type indicators (`/` for directories, `*` for executables, `@` for symlinks)
- `-R`: recursive directory listing
- `-r`: reverse sort order
- `-t`: sort by modification time
- `-S`: sort by file size
- `-1`: force single-column output
- `--no-color`: disable ANSI color escapes
- `--output json`: emit JSON metadata for each entry
- `--output csv`: emit CSV metadata with a header row
- `--output table`: emit aligned table output
- `--json`, `--csv`, `--table`: shorthand structured-output flags
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Notes
- The implementation is Windows-oriented and reports native NTFS attributes rather than fake POSIX permissions.
- Structured output fields include `name`, `path`, `type`, `size`, `modified`, and `attributes`.
- ANSI color is enabled by default for terminal output.

## Examples

```text
ls -l
ls -laF C:\Windows\System32
ls -ltr C:\Projects
ls -1S E:\Backups
ls --output json src | jq '.[].name'
ls --csv src > files.csv
ls -R --table src | more
```

## UNIX origin
A core Unix directory-listing utility from early BSD and System V releases.
