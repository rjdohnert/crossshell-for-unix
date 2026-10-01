# pr

## What it does
Formats text files into paginated output.

## Usage
`pr [OPTION]... [FILE]...`

## Options
- `-l LENGTH`: set page length
- `-w WIDTH`: set output width
- `-h HEADER`: override page header title
- `-t`: omit headers and trailers
- `--help`: show help text
- `--version`: show version information

## Notes
This implementation emits form-feed (`\f`) page separators between pages/files.

## UNIX origin
Classic Unix print/pagination utility.
