# killall

Terminate all processes whose executable names match one or more names.

## Synopsis

killall [options] name...

## Options

- `-i`, `--ignore-case`: ignore case distinctions
- `-e`, `--echo`: print each process that was terminated

## Exit Status

- `0`: at least one matching process terminated
- `1`: no matching process terminated
- `2`: usage or enumeration error

## Examples

```powershell
killall notepad.exe
killall -i -e cmd.exe conhost.exe
```
