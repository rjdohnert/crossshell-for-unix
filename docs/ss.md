# ss

Show socket statistics (TCP/UDP) with local/peer endpoints and owning PID.

## Synopsis

ss [options]

## Options

- `-t`, `--tcp`: show TCP sockets only
- `-u`, `--udp`: show UDP sockets only
- `-l`, `--listening`: show listening sockets
- `-n`, `--numeric`: numeric output (default)
- `-h`, `--help`: show help
- `--version`: show version

## Examples

```powershell
ss
ss -t
ss -u -l
```
