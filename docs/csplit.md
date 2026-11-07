# csplit

## What it does
Splits a file into numbered output files at line numbers or lines matching a regular expression. Output files use the `xx00`, `xx01`, ... prefix by default; file-size counts are printed to standard output.

## Options
- `-f PREFIX`, `--prefix=PREFIX`: use a plain filename prefix (path separators are rejected)
- `-b FORMAT`, `--suffix-format=FORMAT`: format the numeric suffix with one `%d`, `%u`, `%x`, or `%X` conversion; an optional width up to 64 and escaped `%%` are supported
- `-n DIGITS`, `--digits=DIGITS`: set the suffix width (1-64; default 2)
- `-k`, `--keep-files`: keep generated files when an error occurs
- `-z`, `--elide-empty-files`: remove zero-length output files
- `-s`, `-q`, `--silent`, `--quiet`: suppress file-size counts
- `-h`, `-?`, `--help`: show help
- `-V`, `--version`: show version
- `--`: stop option parsing

## Patterns
- `N`: split before line `N`
- `/REGEXP/[OFFSET]`: split at a matching line, shifted by the optional signed line offset
- `%REGEXP%[OFFSET]`: omit the matching line and split at its offset
- `{N}`: repeat the preceding pattern `N` additional times
- `{*}`: repeat the preceding pattern as many times as it matches

Regular expressions support `^`, `$`, `.`, `*`, escaped characters, and bracket classes with ranges and `^` negation. Negative offsets buffer lines until the split position is known; allocation or I/O errors fail the operation and remove files created by the run unless `-k` is specified.

Output files are created exclusively, so an existing file with the same generated name is never truncated.

## UNIX origin
A classic Unix file-splitting utility from the System V era.
