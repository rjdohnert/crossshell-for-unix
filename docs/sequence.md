# sequence

## What it does
Sorts lines from one or more files, or from standard input, using Unix-style sort semantics adapted for Windows.

## Usage
```text
sequence [OPTION]... [FILE]...
sequence [OPTION]... -
```

## Options
- `-b`, `--ignore-leading-blanks`: ignore leading blanks.
- `-d`, `--dictionary-order`: compare only blanks and alphanumeric characters.
- `-f`, `--ignore-case`: fold lower-case characters to upper-case for comparison.
- `-g`, `--general-numeric-sort`: compare using general numeric values.
- `-h`, `--human-numeric-sort`: compare human-readable numbers like `2K` and `1G`.
- `-i`, `--ignore-nonprinting`: ignore non-printing characters.
- `-M`, `--month-sort`: sort by month names.
- `-n`, `--numeric-sort`: compare according to numeric values.
- `-R`, `--random-sort`: shuffle values while grouping identical keys.
- `-r`, `--reverse`: reverse sort order.
- `-V`, `--version-sort`: compare version-like numeric segments naturally.
- `-c`, `--check`: check whether input is already sorted.
- `-C`, `--check=silent`: check quietly without reporting the first disorder.
- `-k`, `--key=KEYDEF`: sort by a specific field/character key.
- `-o`, `--output=FILE`: write sorted output to a file.
- `-s`, `--stable`: stabilize sort order.
- `-t`, `--field-separator=SEP`: specify a field delimiter.
- `-u`, `--unique`: keep only the first of identical elements.
- `-z`, `--zero-terminated`: treat NUL as the line terminator.
- `--help`: show help and exit.
- `--version`: show version and exit.

## Examples
```text
sequence names.txt
sequence -u names.txt
sequence -t: -k3,3n /etc/passwd
sequence -h -r -k2 disk_usage.txt -o sorted_usage.txt
```

## UNIX origin
This utility follows the general contract of the traditional Unix `sort` command, adapted for Windows-native file and console workflows.
