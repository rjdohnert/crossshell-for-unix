# tcsh

## What it does
Provides a tcsh-style interactive shell with command history, aliases, job control, completion, control-flow features, and interactive help.

This implementation is designed to feel like a traditional `tcsh` session while remaining Windows-native.

## Usage
```text
tcsh [options] [script [args...]]
```

## Options
- `-h`, `--help`: show help text and shell command overview
- `--version`: show version information

## Highlights
- Interactive command entry with history
- Alias definitions and shell variables
- Job control commands
- Built-in `help` and `builtins` output
- Shell-style control flow and expression handling

## Built-in Help
The shell includes help topics for common commands and utilities, and `help` can be used interactively to inspect built-ins or user-defined functions.

## Examples
- `tcsh`
- `tcsh script.csh`
- `tcsh -h`
- `tcsh --version`

## Notes
- `help` or `builtins` inside the shell shows the built-in command catalog.
- This shell is intended for compatibility and scripting workflows rather than strict historical binary compatibility.

## UNIX origin
A classic C shell descendant from BSD Unix and later System V and Unix-compatible environments.