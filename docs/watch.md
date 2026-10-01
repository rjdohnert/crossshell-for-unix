# watch

Run a command repeatedly and show refreshed output.

## Synopsis

watch [options] command [args...]

## Options

- `-n`, `--interval SEC`: refresh interval in seconds (default `2.0`)
- `-t`, `--no-title`: hide header line
- `-h`, `--help`: show help
- `--version`: show version

## Notes

- Press `Esc` to exit the watch loop.
- The command is executed through `cmd.exe /c` for broad Windows command compatibility.

## Examples

```powershell
watch -n 1 vmstat
watch -n 2 "ps"
watch -t -n 0.5 "bdf"
```
