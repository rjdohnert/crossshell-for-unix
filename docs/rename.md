# rename

## What it does

Renames files by replacing a pattern in each file's base name. Directory paths are preserved. The command supports literal substring replacement, ECMAScript regular expressions, and Windows wildcard expansion.

## Usage

```text
rename [OPTIONS] <expression> <replacement> <file...>
rename -h | --help
rename -V | --version
```

## Options

- `-v`, `--verbose`: show each rename operation and diagnostic skip message.
- `-n`, `--no-act`, `--dry-run`: preview changes without modifying the filesystem.
- `-i`, `--interactive`: prompt before overwriting an existing destination.
- `-o`, `--no-overwrite`: skip files whose destination already exists.
- `-a`, `--all`: replace every occurrence of the expression.
- `-l`, `--last`: replace only the last occurrence. Ignored with `--all` or `--regex`.
- `-c`, `--ignore-case`: perform case-insensitive matching.
- `-e`, `--regex`: interpret the expression as an ECMAScript regular expression.
- `-h`, `--help`: display the complete help text.
- `-V`, `--version`: display version information.

Short options can be bundled, for example `-van`.

## Behavior

- Without `--regex`, the expression is treated as a literal substring and only its first occurrence is replaced by default.
- With `--regex`, replacement strings can reference capture groups such as `$1` and `$2`.
- File arguments containing `*` or `?` are expanded by `rename`, because Windows command shells do not normally expand them.
- Only the base name is transformed; the parent directory remains unchanged.
- Existing destinations are overwritten by default. Use `--no-overwrite` or `--interactive` to control collisions.
- Renaming a file only to change its case is supported on Windows.

## Examples

```text
rename .jpeg .jpg *.jpeg
rename -v -n draft final draft_*.txt
rename -a _ - *.*
rename -l old new my_old_doc_old.txt
rename -i -c data DATA data*.csv
rename -e "([a-z]+)_([0-9]+)" "$2_$1" *.log
```

## Exit status

- `0`: all requested operations completed successfully, including a dry run.
- `1`: one or more file operations failed.
- `2`: invalid options, insufficient operands, or an invalid regular expression.

## UNIX origin

A pattern-based file-renaming utility found in Unix and HP-UX environments.