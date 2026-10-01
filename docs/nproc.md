# nproc

Print the number of available processing units.

## Synopsis

nproc [options]

## Options

- `--all`: print all installed processing units
- `--ignore=N`: subtract `N` from the printed count
- `-h`, `--help`: show help
- `--version`: show version

## Examples

```powershell
nproc
nproc --all
nproc --ignore=1
```
