# gsar

`gsar` searches and optionally replaces byte patterns in text and binary files.
Input is processed as binary data, so matches may include null bytes or cross
line and buffer boundaries.

## Usage

```text
gsar -sPATTERN [-rPATTERN] [options] [input [output]]
gsar -sPATTERN -F [options] < input > output
```

Use `-s PATTERN` or `--search PATTERN` to separate the search option from its
value. Without `-r`, matches are passed through unchanged. A bare `-r` deletes
matches; `-rPATTERN` or `--replace PATTERN` supplies replacement bytes.

One input and one output can be supplied positionally, or with `-oFILE` /
`--output FILE`. Without an output path, replacement edits each input file
in-place. Existing separate output files are not overwritten unless `-f` or
`--force` is specified. Writes use a temporary file in the destination
directory and atomically install the completed result. In-place replacement
refuses symbolic links and Windows reparse points.

If no input files are given, `gsar` reads standard input and writes transformed
data to standard output. `-F` / `--filter` explicitly selects this mode.
Diagnostics and match summaries are written to standard error.

## Options

| Option | Description |
| --- | --- |
| `-sPATTERN`, `-s PATTERN`, `--search PATTERN` | Required search pattern. |
| `-rPATTERN`, `--replace PATTERN` | Replace each match. |
| `-r` | Delete each match. |
| `-i`, `--ignore-case` | Case-insensitive ASCII byte matching. |
| `-c`, `--count` | Count matches without writing transformed data. |
| `-l`, `--list` | List only input files containing a match. |
| `-b`, `--offsets` | Print match offsets in hexadecimal. |
| `-x`, `--hex` | Print each matching byte sequence in hexadecimal. |
| `-v`, `--verbose` | Print per-file match summaries. |
| `-f`, `--force` | Allow replacing an existing separate output file. |
| `-F`, `--filter` | Read standard input and write standard output. |
| `-oFILE`, `--output FILE` | Write one input file to a separate output file. |
| `-u`, `--backup` | Create a `.bak` backup before in-place replacement. |
| `-BEXT`, `--backup-ext EXT` | Create a backup using the supplied suffix. |
| `-h`, `--help`, `/?` | Display help. |
| `-V`, `--version` | Display version identification. |

`-c` cannot be combined with replacement or an output file. `-l` is for
search-only file input. Backups are only available for in-place replacement;
an existing backup is never overwritten. File searches report the number of
matches when at least one match is found; `-v` also reports files with no
matches.

## Pattern escapes

The following escapes are recognized in both search and replacement patterns:

| Escape | Byte |
| --- | --- |
| `\a`, `\b`, `\e`, `\f`, `\n`, `\r`, `\t`, `\v` | ASCII control bytes |
| `\\` | Literal backslash |
| `\OOO` | Octal byte, up to three digits and at most 255 |
| `\xHH` | One hexadecimal byte |
| `\dDDD` | Decimal byte, up to three digits and at most 255 |
| `::` | Literal colon |
| `:n`, `:r`, `:t`, `:b`, `:a`, `:e`, `:0` | Newline, carriage return, tab, backspace, bell, escape, null |
| `:xHH...` | One or more complete hexadecimal byte pairs |
| `:DDD` | Decimal byte from 0 to 255 |

Invalid numeric escapes are rejected rather than truncated or wrapped.

## Examples

Replace CR/LF with LF and write a separate output:

```text
gsar -s\r\n -r\n input.txt output.txt
```

Replace text in-place, saving the original as `source.cpp.bak`:

```text
gsar -i -sold -rnew -u source.cpp
```

Search a binary signature and print matching bytes:

```text
gsar -s:xDEADBEEF -b -x binary.exe
```

Use GSAR as a filter:

```text
type input.txt | gsar -sfoo -rbar -F > output.txt
```

## Exit status

- `0`: operation completed successfully, including searches with no matches.
- `1`: invalid arguments or an input, output, or processing error.

## Build

Run `src/gsar/build.ps1` on Windows to build `gsar.exe` and install it to the
repository's `bin` directory.
