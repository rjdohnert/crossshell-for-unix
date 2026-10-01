# mtr

## What it does

Continuously traces the IPv4 route to a host with ICMP Echo probes and displays rolling latency and packet-loss statistics for each hop.

The display refreshes in place for interactive monitoring. A fixed-cycle report mode is available for scripts and one-shot diagnostics.

## Usage

```text
mtr [options] <HOSTNAME or IP>
```

## Options

- `-h`, `--help`: display help and exit
- `-v`, `--version`: display the version and exit
- `-c COUNT`, `--report-cycles COUNT`: stop after `COUNT` probe cycles; minimum 1
- `-m HOPS`, `--max-ttl HOPS`: set the maximum hop count from 1 through 255; default 30
- `-i SECONDS`, `--interval SECONDS`: set the delay between cycles from 0.01 through 86400 seconds; default 1.0
- `-t MS`, `--timeout MS`: set the timeout for each ICMP probe from 1 through 60000 milliseconds; default 1000
- `-s BYTES`, `--psize BYTES`: set the ICMP payload size from 0 through 65500 bytes; default 32
- `-n`, `--no-dns`: display numeric addresses without reverse DNS lookups

## Interactive keys

- `q`: quit
- `r`: reset collected statistics
- `n`: toggle reverse DNS lookup
- `p`: pause or resume probing

## Output columns

- `Host`: hop number, responder address, and optional reverse-DNS name
- `Loss%`: percentage of probes without a usable response
- `Snt`: probes sent to the hop
- `Last`: latest round-trip time in milliseconds
- `Avg`: average round-trip time
- `Best`: lowest round-trip time
- `Wrst`: highest round-trip time
- `StDev`: round-trip-time standard deviation

## Examples

- `mtr microsoft.com`
- `mtr -n -c 10 1.1.1.1`
- `mtr --interval 0.5 --max-ttl 15 cloudflare.com`

## Exit status

- `0`: the requested run completed or the user quit normally
- `1`: invalid arguments, name-resolution failure, or initialization failure

## Notes

- This implementation probes IPv4 destinations through the Windows ICMP API.
- Reverse DNS is cached by responder address and may add latency when a hop is first observed.
- The interactive display requires a console that supports VT100/ANSI escape processing.

## UNIX origin

`mtr` is modeled after My Traceroute, combining route discovery with continuously updated ping-style statistics.
