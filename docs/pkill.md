# pkill

Terminate processes selected by executable-name pattern.

## Synopsis

pkill [options] pattern

## Options

- `-i`, `--ignore-case`: ignore case distinctions
- `-x`, `--exact`: require exact name match
- `-e`, `--echo`: print each process that was terminated

## Exit Status

- `0`: at least one matching process terminated
- `1`: no matching process terminated
- `2`: usage or enumeration error

## Examples

```powershell
pkill notepad
pkill -x -e cmd.exe
pkill -i powershell
```
