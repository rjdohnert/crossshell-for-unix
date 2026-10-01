# bat

## What it does

Displays files or standard input with syntax highlighting, optional line numbers, and grid decorations.

## Arguments

- `<FILE>...`: file(s) to process and display
- `-`: read directly from standard input
- If no file is specified, `bat` reads from standard input

## Options

- `-A`, `--show-all`: show non-printable characters (tabs as →, spaces as •, EOL as ↵)
- `-f`, `--force`: force display of binary files
- `-l`, `--language <LANG>`: explicitly set the language for syntax highlighting
- `-n`, `--number`: only show line numbers, without grid borders
- `-p`, `--plain`: show plain text only without grid, line numbers, or colors
- `--style <STYLE>`: set custom display style [full, plain, numbers, header, grid]
- `-h`, `--help`: display the detailed help screen
- `-V`, `--version`: display version information

## Supported languages

cpp, python, javascript, html, xml, batch, powershell, shell, rust, go, java, sql, css

## Examples

```text
bat main.cpp
bat main.h main.cpp
ipconfig | bat -l batch
bat -A script.py
bat -p config.json
```

## Notes

- `bat` can be used in pipelines and is designed to read from standard input when no file is given.
- Use `-p` for plain output in scripts or redirects.

## UNIX origin

This is a Windows-oriented alternative to the common `bat`/`batcat` style file viewer rather than a direct copy of one specific historical Unix release.
