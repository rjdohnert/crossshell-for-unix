# pgrep

Find process IDs by executable-name pattern.

## Synopsis

pgrep [options] pattern

## Options

- `-i`, `--ignore-case`: ignore case distinctions
- `-x`, `--exact`: require exact name match
- `-l`, `--list-name`: print `PID NAME`
- `-c`, `--count`: print count only

## Exit Status

- `0`: one or more matches found
- `1`: no matches found
- `2`: usage or enumeration error

## Examples

```powershell
pgrep cmd.exe
pgrep -i -x powershell.exe
pgrep -l ssh
pgrep -c conhost
```
