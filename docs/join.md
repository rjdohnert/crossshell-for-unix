# join

## What it does
Joins lines from two files by a common key field.

## Usage
`join [OPTION]... FILE1 FILE2`

## Options
- `-1 FIELD`: join field in `FILE1` (1-based)
- `-2 FIELD`: join field in `FILE2` (1-based)
- `-t CHAR`: field delimiter character
- `-a FILENO`: include unpairable lines from file `1` or `2`
- `-v FILENO`: output only unpairable lines from file `1` or `2`
- `--help`: show help text
- `--version`: show version information

## Notes
Input files should be sorted on their respective join keys for deterministic output.

## UNIX origin
Standard Unix relational join utility.
