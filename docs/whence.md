# whence

## What it does

Resolves command names to either shell built-ins or filesystem executables.

For each target name, whence can:

- identify shell built-ins
- resolve executable paths using current directory, PATH, and PATHEXT rules
- print the first match or all matches in search order

## Synopsis

whence [-v] [-a] [-p] name ...

whence [--] name ...

whence [-h | --help | /?]

## Resolution order

For each target, search proceeds in this order:

1. shell built-ins (unless path-only mode is enabled)
2. current directory
3. PATH directories, using PATHEXT extension expansion when needed

When a target has no executable extension, whence tries extensions from PATHEXT (for example .COM, .EXE, .BAT, .CMD).

## Options

- `-v`, `--verbose`: verbose classification output
- `-a`, `--all`: report all matches instead of stopping at first
- `-p`, `--path-only`: skip shell built-in checks and search executables only
- `-h`, `--help`, `/?`: show help text
- `--`: end of options; treat all following values as targets

## Output behavior

Default mode:

- built-in match: `name is a shell builtin`
- executable match: absolute executable path
- not found: no output line for that target

Verbose mode:

- built-in match: `name is a shell builtin`
- executable match: `name is C:\path\to\tool.exe`
- not found: `name not found`

## Shell built-ins recognized

The utility recognizes cmd.exe built-ins including:

assoc, break, call, cd, chdir, cls, color, copy, date, del, dir, echo, endlocal, erase, exit, for, ftype, goto, if, md, mkdir, mklink, move, path, pause, prompt, rd, ren, rename, rmdir, set, setlocal, shift, start, time, title, type, ver, verify, vol

## Exit status

- 0: all specified names resolved
- 1: one or more names were not found, or invalid option/usage

## Examples

Locate one command:

```text
whence notepad
```

Verbose lookup for multiple targets:

```text
whence -v cd python unknown_cmd
```

Show all executable matches:

```text
whence -a python
```

Search only filesystem paths (skip built-ins):

```text
whence -p dir
```

Search for a target that starts with a dash:

```text
whence -- -v
```

## Notes

- Use `-a` with `-p` to enumerate all executable candidates for names that also exist as built-ins.
- For explicit paths (for example .\tool or C:\bin\tool), whence checks the path directly and applies PATHEXT if no valid extension is present.

## UNIX origin

A Korn shell command-introspection utility used to identify where commands resolve and whether they are built-ins or external executables.
