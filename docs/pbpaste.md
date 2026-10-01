# pbpaste

## What it does
Writes text, formatted data, or copied file paths from the Windows clipboard to standard output in UTF-8.

## Options
- `-Prefer <format>`, `-prefer <format>`, `--prefer <format>`: select `txt`, `text`, `rtf`, `html`, `files`, or `file`
- `-pboard <name>`: accept a macOS pasteboard name for script compatibility
- `-n`, `--no-newline`: remove trailing newline characters
- `-u`, `--unix`: normalize line endings to LF
- `-w`, `--windows`, `--dos`: normalize line endings to CRLF
- `-r`, `--raw`: write the clipboard payload without newline normalization or trimming
- `-h`, `--help`: show help text
- `-v`, `-V`, `--version`: show version information

When RTF, HTML, or copied files are requested but unavailable, `pbpaste` falls back to Unicode plain text. An empty clipboard produces no output and exits successfully.

## Examples
```text
pbpaste
pbpaste -u > script.sh
pbpaste -Prefer html > snippet.html
pbpaste -Prefer files
```

## UNIX origin
A Unix-style clipboard utility associated with the Apple macOS lineage rather than a single classic System V command.