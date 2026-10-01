# netctl

## What it does
Captures and analyzes network traffic on Windows using either a raw Winsock engine (IP-layer) or an optional Npcap engine (Ethernet Layer-2).

It supports protocol and port filtering, live formatted output, optional hex payload previews, and binary PCAP export for tools like Wireshark.

## Usage
- `netctl list`
- `netctl capture -i <INTERFACE_IP> [options]`
- `netctl help`

## Commands
- `list`: show active IPv4/IPv6 adapter addresses you can capture from
- `capture`: start packet capture on the specified interface IP
- `help`: show built-in command help

## Capture Options
- `-i`, `--interface <IP>`: required interface address to bind and capture from
- `-p`, `--proto <NAME>`: protocol filter (`ALL`, `TCP`, `UDP`, `ICMP`, `DNS`, `HTTP`)
- `--port <NUMBER>`: port filter (`1..65535`)
- `-o`, `--output <FILE>`: append plain-text output to file
- `--pcap <FILE>`: write capture to binary PCAP
- `--engine <winsock|npcap>`: backend selection (default `winsock`)
- `-c`, `--count <NUM>`: stop after `NUM` matching packets (`0` means unlimited)
- `-v`, `--verbose`: include extra TCP details (sequence, ack, window)
- `-x`, `--hex`: include payload hex+ASCII dump (truncated)
- `--no-color`: disable ANSI colors in console output

## Engine Behavior
- `winsock`:
  - Captures at IP layer via raw sockets.
  - Uses `recvfrom` source endpoint metadata for IPv6 headerless payload cases seen on some Windows configurations.
  - PCAP link type for exported packets: `DLT_RAW (101)`.
- `npcap`:
  - Available when built with `NETCTL_ENABLE_NPCAP` and linked with `wpcap.lib` and `Packet.lib`.
  - Captures at Ethernet Layer-2, including VLAN-tagged frames.
  - PCAP link type for exported packets: `DLT_EN10MB (1)`.

## Filter Notes
- `--proto DNS` maps to DNS-over-UDP inspection (port 53 patterns).
- `--proto HTTP` maps to request-line heuristics on common HTTP ports (`80`, `8080`).
- When both `--proto` and `--port` are set, both filters are applied.

## Security and Robustness Notes
- Requires Administrator privileges for packet capture.
- Console Ctrl+C is handled for graceful shutdown.
- HTTP request line previews are sanitized before terminal output.
- IPv6 extension header traversal is depth-capped to bound parser work.

## Examples
- `netctl list`
- `netctl capture -i 192.168.1.10`
- `netctl capture -i fe80::abcd:1234%12 -p ICMP`
- `netctl capture -i 10.0.0.5 -p TCP --port 443 -v -c 200`
- `netctl capture -i 10.0.0.5 --pcap traffic.pcap`
- `netctl capture -i 10.0.0.5 --engine npcap --pcap l2_capture.pcap`
- `netctl capture -i 10.0.0.5 -o session.log --no-color`

## Exit Status
- `0`: successful command execution
- `1`: invalid arguments, missing privileges, or runtime capture/setup failure

## UNIX origin
`netctl` is a CrossShellUX utility and not a direct historical UNIX command. It follows familiar UNIX-style CLI conventions while targeting Windows packet capture APIs.
