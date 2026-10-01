# strings

Extract printable text sequences from binary files.

## Synopsis

strings [options] file...

## Options

- `-n`, `--bytes N`: minimum string length (default `4`)
- `-h`, `--help`: show help
- `--version`: show version

## Examples

```powershell
strings program.exe
strings -n 8 archive.bin
```
