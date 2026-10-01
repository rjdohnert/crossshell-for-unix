# cat

## What it does
Concatenates and prints files to standard output, with HP-UX 11i formatting options for line numbering, character visualization, and blank-line suppression.

## Supported options
- `-b`: number lines (omit line numbers from blank lines)
- `-e`: print $ at end of each line (implies `-v`)
- `-n`: number all output lines
- `-r`: squeeze consecutive blank lines into one
- `-s`: silent mode (suppress error messages about missing files)
- `-t`: display tabs as `^I` and form-feeds as `^L` (implies `-v`)
- `-u`: unbuffered output (character-by-character)
- `-v`: display non-printing characters visibly
- `--json`: emit one line-delimited JSON object per input line as `{"line":N,"content":"..."}`
- `-h`, `--help`: show help text

## Behavior
- If no files are specified or `-` is given, cat reads from standard input.
- When `-b` is specified, it automatically enables `-n`.
- When `-e` or `-t` are specified, they automatically enable `-v`.
- Control characters display as `^X` (e.g., `^M` for CR), DEL displays as `^?`, and high-bit characters display as `M-x`.
- Tab, newline, and form-feed characters are handled specially and exempt from `-v` display unless `-t` is also used.
- In `--json` mode, the output is structured as one JSON object per line rather than the usual formatted text, which is useful for PowerShell and pipeline consumers.

## Examples

```text
cat file1.txt file2.txt
cat -n source.cpp
cat -ben main.c
cat -vte binary_dump.dat
cat -s missing.txt valid.txt > combined.txt
cat --json file.txt
```

## UNIX origin
A fundamental Unix file-display utility from early BSD and System V releases. This implementation follows the authentic HP-UX 11i specification with System V flag behaviors.
