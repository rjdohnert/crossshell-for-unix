# a2pdf

## What it does

Converts plain text (including UTF-8 input) into PDF 1.4 or PostScript Level 3 documents.

It supports line wrapping, optional line numbers, tab expansion, custom
font/size, portrait/landscape layout, and document title headers.

## Usage

- `a2pdf [OPTIONS] [INPUT_FILE]`

If `INPUT_FILE` is omitted or set to `-`, input is read from standard input.

## Options

- `-o`, `--output <file>`: output path (default derived from input name)
- `-p`, `--postscript`: generate `.ps` instead of `.pdf`
- `-f`, `--font <name>`: font family (`Courier`, `Helvetica`, `Times-Roman`)
- `-s`, `--font-size <pt>`: font size (default: `10.0`)
- `-l`, `--line-numbers`: prepend line numbers
- `--landscape`: landscape page orientation
- `--no-wrap`: disable automatic line wrapping
- `--title <text>`: document title shown in page header
- `--tab-size <spaces>`: tab expansion size (default: `4`)
- `-h`, `--help`, `/?`: show help
- `-v`, `--version`: show version

## Behavior Notes

- Default output extension is `.pdf`; with `--postscript`, default becomes `.ps`.
- UTF-8 is decoded and mapped to WinAnsi-compatible output where possible.
- Unsupported glyphs are replaced with `?`.
- Wrapped continuation lines include visual continuation indentation.

## Examples

- `a2pdf document.txt`
- `a2pdf document.txt -o document.pdf`
- `a2pdf -p --landscape server.log -o server_log.ps`
- `a2pdf -l -f Courier -s 9 source.cpp -o source.pdf`
- `type notes.txt | a2pdf - --title "Session Notes" -o notes.pdf`

## Exit Status

- `0`: success
- `1`: output generation failed (for example, write/open failure)

## UNIX origin

`a2pdf` is a CrossShellUX utility and not a historical UNIX standard command.
