# grep

## What it does
Searches input for lines matching a pattern.

## Options
- `-E`: use extended regular expressions (default syntax otherwise is basic regular expressions)
- `-F`: treat patterns as fixed strings
- `-e pattern`: add a pattern (repeatable)
- `-f file`: read patterns from file (repeatable)
- `-m num`, `--max-count=num`: stop after selecting `num` lines per input
- `--max-count num`: same as `--max-count=num`
- `-i`: ignore case
- `-y`: same as `-i` (traditional alias)
- `-w`: select only whole-word matches
- `-v`: invert the match
- `-n`: print line numbers
- `-b`: print 512-byte block numbers for selected lines
- `-c`: print only counts of selected lines
- `-l`: print only file names with matches
- `-q`: quiet mode, suppress normal output and use exit status
- `-o`: print only the matching part of selected lines
- `-x`: match only whole lines
- `-h`: suppress file name prefixes
- `-H`: force file name prefixes
- `-r`, `-R`: recurse into directories
- `-s`: suppress error messages
- `--help`: show help text
- `-V`, `--version`: show version information

## Usage
- `grep [-E | -F] [-c | -l | -q] [-binswvyxohH] [-m num] [-e pattern]... [-f file]... [pattern] [file ...]`

## Invocation Compatibility
- When invoked as `egrep`, extended regular expressions are enabled by default (equivalent to `grep -E`).
- When invoked as `fgrep`, fixed-string matching is enabled by default (equivalent to `grep -F`).

## Exit Status
- `0`: one or more selected lines were found
- `1`: no selected lines were found
- `2`: an error occurred

## Option Interactions
- `-q` overrides output-oriented flags (`-l`, `-c`, `-o`) and suppresses normal output.
- `-l` overrides `-c` and `-o`.
- `-c` overrides `-o`.

## UNIX origin
A foundational Unix text-search utility from early BSD and System V releases.
