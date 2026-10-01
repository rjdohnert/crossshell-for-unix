# entab

## What it does

Converts runs of spaces, and eligible tabs, to tabs in files or standard input.

## Usage

```text
entab [OPTION]... [FILE]...
```

With no file, or when the file name is `-`, `entab` reads standard input. A bare interactive invocation with no arguments displays help instead of waiting for input.

## Options

- `-a`, `--all`: convert all eligible runs of spaces, not only leading whitespace.
- `--first-only`: convert only leading whitespace and override `--all`.
- `-t N`, `--tabs=N`: use tab stops `N` characters apart and enable all-region conversion unless `--first-only` is active.
- `-t LIST`, `--tabs=LIST`: use comma- or space-separated explicit 1-based tab-stop columns and enable all-region conversion unless `--first-only` is active.
- `-tLIST`: attached short-option form for a tab specification.
- `-N`: shorthand for `-t N`.
- `-h`, `--help`: show help text and exit.
- `-v`, `--version`: show version information and exit.
- `--`: treat following arguments as file names even when they begin with `-`.

## Behavior

- Leading whitespace is converted by default.
- With `--all`, eligible whitespace runs anywhere in the line are converted.
- Explicit tab-stop lists use 1-based column positions.
- Input and output use binary mode to preserve line endings.
- Multiple files are processed in order.
- Invalid options, missing tab values, invalid tab specifications, and input-file failures return non-zero status.
- Broken output destinations terminate cleanly.

## Examples

```text
entab source.txt
entab -a source.txt
entab -t 4 source.txt
entab --tabs=4,8,12 source.txt
entab --first-only -t 4 source.txt
type source.txt | entab
```

## UNIX origin

A standard Unix text-formatting utility commonly paired with `expand` to restore tab-based indentation.
