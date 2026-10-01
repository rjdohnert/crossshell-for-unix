# dirtree

## What it does

Visualizes a directory hierarchy as a tree with ANSI color coding, Unicode box-drawing characters, optional file sizes, and extension filtering.

## Usage

- `dirtree [OPTIONS] [PATH]`

## Options

- `-a`, `--all`: include hidden files and system folders
- `-d`, `--dirs-only`: list directories only
- `-L`, `--level <LEVEL>`: set the maximum directory recursion depth
- `-s`, `--sizes`: print human-readable file sizes alongside filenames
- `-e`, `--extension <EXT>`: filter files by extension (repeatable)
- `--ascii`: use ASCII branch characters instead of Unicode box-drawing glyphs
- `-c`, `--color <when>`: control color output [always, never, auto]
- `-h`, `--help`: print the comprehensive help message
- `-V`, `--version`: print version information

## Notes

- Directories are sorted before files; both groups are sorted alphabetically (case-insensitive).
- Color coding: cyan for directories, green for executables, red for archives, yellow for source files, magenta for symlinks.
- A summary line of directory and file counts is printed after the tree.
- `--color auto` enables color only when stdout is a terminal.
- Hidden entries are skipped unless `-a` is supplied.

## Examples

- `dirtree`
- `dirtree C:\Projects\App`
- `dirtree -L 2 C:\Projects\App`
- `dirtree -a -d`
- `dirtree -e cpp -e hpp -s`
- `dirtree --ascii -L 3`
- `dirtree --color never`
- `dirtree --help`
- `dirtree --version`

## UNIX origin

Inspired by the `tree` command available on Unix/Linux systems.
