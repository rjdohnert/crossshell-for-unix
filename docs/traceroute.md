# traceroute

## What it does
Discovers the path packets take to a network host by sending probe packets with increasing time-to-live values.

This implementation supports UDP-based probes by default and an ICMP mode for environments where that is preferred.

## Usage
```text
traceroute [-n] [-I] [-m max_ttl] [-f first_ttl] [-p port] [-q nqueries] [-w waittime] [-s src_addr] host
```

## Options
- `-n`: numeric output only, disable reverse DNS lookups
- `-I`: use ICMP Echo Request probes instead of UDP datagrams
- `-m max_ttl`: set the maximum number of hops to probe
- `-f first_ttl`: set the starting hop value
- `-p port`: choose the base UDP destination port
- `-q nqueries`: number of probes to send per hop
- `-w waittime`: response timeout in seconds
- `-s src_addr`: bind probes to a specific local source address
- `-h`, `-?`, `--help`: show help text

## Status Markers
- `*`: probe timed out without an ICMP reply
- `!N`: host unreachable
- `!P`: port unreachable, usually indicating the destination was reached with UDP probes
- `!S`: source route failed

## Examples
- `traceroute www.example.com`
- `traceroute -n -I 8.8.8.8`
- `traceroute -m 15 -w 1 192.168.1.1`

## Notes
- Administrator privileges may be required for raw packet handling.
- The tool prints intermediate hops, round-trip timing, and responder addresses when available.

## UNIX origin
A classic Unix network diagnostic utility from BSD and later Unix-like systems.