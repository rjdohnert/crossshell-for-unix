# du

`du` estimates file and directory usage using filesystem allocation data by
default, or apparent file sizes when requested.

## Usage

```text
du [OPTIONS] [FILE...]
```

With no file operands, `du` scans the current directory. Use `-` as an operand
to read newline-delimited paths from standard input.

## Options

| Option | Description |
| --- | --- |
| `-a`, `--all` | Include individual files as well as directory totals. |
| `-s`, `--summarize` | Display one summary per operand. |
| `-c`, `--total` | Display a cumulative grand total. |
| `-b`, `--bytes` | Report apparent sizes in bytes. |
| `--apparent-size` | Use apparent sizes in the selected display unit. |
| `-0`, `--null` | Terminate human-readable output entries with NUL. |
| `-L`, `--dereference` | Follow symbolic links and Windows reparse points. Active directory cycles and duplicate file identities are guarded. |
| `-d N`, `-d=N`, `--max-depth=N` | Display directory totals only through depth N. N must be non-negative. |
| `-k` | Display 1024-byte blocks (default). |
| `-m` | Display 1048576-byte blocks. |
| `-H`, `--human-readable` | Display sizes using readable units such as `1K` or `2G`. |
| `--exclude=PATTERN` | Exclude files and directories whose names match the wildcard pattern. |
| `--files0-from=FILE` | Read NUL-delimited paths from FILE; use `-` for standard input. Cannot be combined with file operands. |
| `--json`, `--csv`, `--table` | Emit structured JSON, CSV, or tab-separated output. Select only one. |
| `--pipe COMMAND` | Send the selected output format to COMMAND's standard input. |
| `-h`, `--help` | Display help and exit. |
| `-v`, `--version` | Display version information and exit. |

Structured output escapes path data and reports entry sizes and totals. Errors
are written to standard error and cause a non-zero exit status. CSV paths are
quoted and guarded against spreadsheet formula interpretation.

Without `-L`, symbolic links and Windows reparse points are skipped. With `-L`,
directory cycles are detected and hard-linked files are counted once. Directory
enumeration and file metadata errors are reported rather than silently treated
as zero-byte results. On Windows, allocated sizes come from filesystem
metadata; on POSIX systems they use allocated 512-byte blocks. Use
`--apparent-size` when logical file sizes are desired instead.

## Examples

```text
du -H C:\Projects
du -a --max-depth=2 .
du -s -c C:\Users\Public C:\Temp
du --exclude=*.obj --json build
du --files0-from=paths.list
```
