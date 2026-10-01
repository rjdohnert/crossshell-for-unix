# awk

## What it does
Processes text records using pattern-action rules, similar to the classic Unix text-processing language.

## Options
- `-F fs`, `-Ffs`: use `fs` as the input field separator
- `-f file`: read awk program from `file` (repeatable; program files are concatenated in argument order)
- `-v var=value`, `-vvar=value`: assign a variable before execution (repeatable; `FS`, `OFS`, `ORS` supported)
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Usage Forms
- `awk [-F fs] [-Ffs] [-v var=value]... 'program' [file ...]`
- `awk [-F fs] [-Ffs] [-v var=value]... -f progfile... [file ...]`

## Compatibility Notes
- This implementation focuses on a Solaris-style subset: `BEGIN`, main pattern/action, and `END` blocks with `print` statements.
- Record counters `NR` and `FNR` are supported, along with `NF`, `$0`, `$1..$n`, and `$NF` in `print` arguments.
- Multiple statements separated by `;` are supported when each statement is a `print` statement.

## UNIX origin
A standard Unix text-processing tool from the Bell Labs tradition, widely available in System V and BSD releases.
