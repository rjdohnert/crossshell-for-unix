# make

## What it does

Reads a Makefile, evaluates targets and dependencies, and executes recipes to build a project. Supports parallel jobs, content-hash-based change detection, macro overrides, and structured build telemetry.

## Options

- `-f FILE`, `--file FILE`: read `FILE` as the makefile
- `-j [N]`, `--jobs [N]`: allow `N` parallel jobs (default: number of CPU cores)
- `-n`, `--dry-run`: print commands without executing them
- `-s`, `--silent`, `--quiet`: do not echo recipes
- `-k`, `--keep-going`: continue building after errors
- `-B`, `--always-build`: unconditionally build all targets
- `-C DIR`, `--directory DIR`: change to `DIR` before reading the makefile
- `-H`, `--hash-check`: use content hashing instead of file timestamps to detect changes
- `--export-compile-commands`: export `compile_commands.json` for Clangd and MSVC IntelliSense
- `--json`: output structured build telemetry in JSON format
- `--no-color`: disable ANSI terminal colors
- `-h`, `--help`, `/?`: show help text
- `-v`, `--version`: show version information

## Usage

- `make [options] [target] [MACRO=val] ...`

## Examples

- `make -j8 -H`
- `make -f Makefile.win CXX=clang++ --export-compile-commands`
- `make -n` — preview what would be built without running anything

## Exit Status

- `0`: success
- `1`: build error or fatal failure

## UNIX origin

A foundational UNIX build automation tool originating with Bell Labs in the 1970s, present in every POSIX-compliant system.
