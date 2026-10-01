# gsar

## What it does

Searches and optionally replaces byte patterns or strings in text and binary files. Treats all files as binary streams so patterns can span line boundaries, contain null bytes, and include arbitrary control characters.

## Options

- `-s<string>`: search string (required)
- `-r[string]`: replacement string; omit the string to delete matches; omit `-r` entirely for search-only mode
- `-i`: ignore case
- `-f`: force overwrite of an existing output file
- `-o`: overwrite input files in place using a temporary file
- `-c[n]`: display match context in ASCII format (default: 20 bytes)
- `-x[n]`: display match context in hex dump format (default: 16 bytes)
- `-b`: display search and replace buffer contents
- `-l`: list file names and match counts only
- `-F`: filter mode, reads from stdin and writes to stdout
- `-h`, `--help`, `/?`: show help text
- `-v`, `--version`: show version information

## Escape Sequences

The `:` character introduces escape sequences in search and replacement strings.

- `::` — literal colon
- `:n` — line feed (0x0A)
- `:r` — carriage return (0x0D)
- `:t` — tab (0x09)
- `:b` — backspace (0x08)
- `:a` — bell (0x07)
- `:e` — escape (0x1B)
- `:0` — null byte (0x00)
- `:xHH` — hexadecimal byte value (e.g. `:x0D`, `:xDEADBEEF`)
- `:DDD` — decimal byte value 0–255 (e.g. `:13`, `:255`)

## Usage

- `gsar [options] [infile(s)] [outfile]`

## Examples

- Convert Unix LF to DOS CR/LF: `gsar -s:n -r:r:n input.txt output.txt`
- Replace text in multiple files in place: `gsar -i -s"old" -r"new" -o *.cpp`
- Delete a binary pattern: `gsar -s:xDEADBEEF -r -o binary.dat`
- Filter mode: `type input.txt | gsar -s"foo" -r"bar" > output.txt`

## Exit Status

- `0`: success
- `1`: error

## UNIX origin

Inspired by the classic gsar utility for general-purpose binary and text search-and-replace operations.
