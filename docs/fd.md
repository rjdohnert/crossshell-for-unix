# fd

## What it does

Searches for files and directories by name pattern with fast recursive traversal, ignore rules, type filtering, and optional command execution for each match.

## Usage

- `fd [FLAGS/OPTIONS] [<pattern>] [<path>...]`

## Arguments

- `<pattern>`: regular expression pattern to match filenames against (optional)
- `<path>...`: directory paths to search in; defaults to the current directory

## Options

- `-H`, `--hidden`: include hidden files and directories in search results
- `-I`, `--no-ignore`: do not respect ignore rules (searches node_modules, .git, build, etc.)
- `-s`, `--case-sensitive`: perform case-sensitive matching
- `-i`, `--ignore-case`: perform case-insensitive matching
- `-F`, `--fixed-strings`: treat the pattern as a literal string instead of a regex
- `-a`, `--absolute`: show absolute paths instead of relative paths
- `-0`, `--print0`: separate search results with a null character (NUL)
- `-d`, `--max-depth <d>`: set maximum directory recursion depth
- `--min-depth <d>`: set minimum directory recursion depth
- `-t`, `--type <type>`: filter results by entry type (`f`, `d`, `l`, `x`)
- `-e`, `--extension <ext>`: filter results by file extension
- `-x`, `--exec <cmd>`: execute a command for each search result
- `-c`, `--color <when>`: control colored output [always, never, auto]
- `-h`, `--help`: print the comprehensive help message
- `-V`, `--version`: print version information

## Exec placeholders

- `{}`: full path (auto-quoted)
- `{/}`: filename (auto-quoted)
- `{//}`: parent directory (auto-quoted)
- `{.}`: path without extension (auto-quoted)

## Examples

```text
fd main
fd -e cpp -e hpp . src
fd -t d test
fd -H config
fd -e log -x del {}
```

## Notes

- Hidden files are included when `-H` is used.
- Ignore rules skip common dependency and build directories unless `-I` is supplied.
- `--min-depth` and `--max-depth` can be combined to narrow traversal.

## UNIX origin

This is a Windows-oriented `fd` implementation inspired by the familiar fast file search utility family.
