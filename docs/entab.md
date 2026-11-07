# entab

`entab` converts eligible runs of spaces and tabs to tab characters. It writes
the result to standard output and does not modify input files.

## Usage

```text
entab [OPTION]... [FILE]...
```

With no file operands, `entab` reads standard input. A `-` operand also reads
standard input. With `--`, subsequent operands are always file names, including
a literal file named `-`. A bare interactive invocation with no options prints
help.

## Options

| Option | Description |
| --- | --- |
| `-a`, `--all` | Convert eligible whitespace runs throughout each line. |
| `--first-only` | Convert only leading whitespace; this takes precedence over `--all` regardless of option order. |
| `-t N`, `--tabs=N` | Use regularly spaced tab stops N columns apart. Enables all-region conversion unless `--first-only` is set. |
| `-t LIST`, `--tabs=LIST` | Use explicit, strictly increasing tab-stop columns, such as `4,8,12` or `"4 8 12"`. |
| `-tLIST` | Attached short-option form for a tab specification. |
| `-N` | Shorthand for `-t N`, for example `-4`. |
| `--json` | Emit one JSON object with the converted text in its `output` field. |
| `--csv` | Emit a CSV header and one quoted output field. |
| `--table` | Emit the converted text as one escaped table cell. |
| `--pipe COMMAND` | Send output to COMMAND's standard input and return an error if writing or the command fails. |
| `-h`, `--help` | Display help and exit. |
| `-v`, `--version` | Display version information and exit. |
| `--` | Treat all following arguments as file names. |

Only one of `--json`, `--csv`, or `--table` may be specified. Structured
formats stream output rather than buffering entire inputs. JSON escapes control
characters; invalid UTF-8 bytes are represented as `\u00XX`. CSV quotes embedded
quotes and prefixes formula-like text with an apostrophe to reduce spreadsheet
formula injection risk. Table output escapes control characters so file
contents cannot create extra rows or columns.

Input and output errors, invalid options, and pipe-command failures produce a
non-zero exit status. File input and standard streams use binary mode on
Windows, preserving line endings and non-text bytes.

## Examples

```text
entab source.txt
entab -a source.txt
entab -t 4 source.txt
entab --tabs=4,8,12 source.txt
entab --first-only -a -t 4 source.txt
entab --json source.txt
type source.txt | entab
```

## UNIX origin

A text-formatting utility commonly paired with `expand` to restore tab-based
indentation.
