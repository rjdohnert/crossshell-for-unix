# search

## What it does

Searches a directory tree for files and directories by name, extension, type, and metadata filters such as file size and modification time.

## Usage

- `search [PATH] [OPTIONS]`

## Arguments

- `PATH`: starting directory to scan; defaults to the current directory (`.`)

## Options

- `-n, --name <pattern>`: match filenames using wildcards such as `*.log` or `doc_?`
- `-r, --regex <regex>`: use an ECMAScript regular expression against the filename
- `-e, --ext <extension>`: filter by file extension such as `.log`, `txt`, or `json`
- `-t, --type <f|d|all>`: restrict output to files, directories, or all entries
- `-s, --case-sensitive`: use case-sensitive matching instead of the default case-insensitive behavior
- `--min-size <size>`: minimum file size such as `500B`, `10KB`, `50MB`, or `2GB`
- `--max-size <size>`: maximum file size
- `--after <date>`: include only items modified on or after `YYYY-MM-DD` or `YYYY-MM-DD HH:MM:SS`
- `--before <date>`: include only items modified on or before the same date formats
- `-d, --depth <N>`: maximum recursion depth; `0` means only inspect the target directory itself
- `--summary`: print only totals and storage statistics instead of per-entry rows
- `-h, --help, /?`: display the help screen

## Examples

```text
search -n "*.log" --min-size 10MB
search C:\Projects -t f -e cpp --after 2024-01-01
search -r "^test_[0-9]+\.json$" -d 2
search --type d -n "*build*"
```

## Notes

- Matching is recursive by default and follows the supplied root path.
- Path filters are applied before output is displayed, which keeps the command useful for large directory trees.
- Date parsing accepts both a date-only value and a timestamp value with a time.
- The command can report a compact summary (`--summary`) for scripting or CI use.

## UNIX origin

This is a Windows-native metadata search utility inspired by traditional Unix-style search workflows, combining recursive directory traversal with file-name and attribute filtering in a single command.
