# nc

## Overview

nc is a BSD-style netcat utility for TCP and UDP connections, listening sockets, port scans, and stream relaying.

## Synopsis

```text
nc [-46dhklnuvz] [-e command] [-i interval] [-p port] [-s source_ip]
   [-w timeout] [destination] [port[s]]
```

## Core modes

- Client mode: connect to destination and one or more ports.
- Listen mode: accept inbound connections on a local port.
- Scan mode: probe port reachability with -z.
- Execute mode: bind a launched command to a network socket with -e.

## Options

- -4: IPv4 only
- -6: IPv6 only
- -d: detach stdin input path
- -e command: execute command using anonymous pipes over socket
- -h, --help, /?: show help
- -i interval: delay between lines/ports (seconds)
- -k: keep listener open for multiple connections
- -l: listen mode
- -n: numeric mode, skip DNS lookups
- -p port: local source port or listen port
- -s source_ip: local source bind address
- -u: UDP mode
- -v: verbose; repeat for extra verbosity
- -w timeout: connect and network I/O timeout (seconds)
- -z: zero-I/O scan mode

## Port input

- Single ports: 80
- Ranges: 20-80
- Service names are accepted where resolvable by getservbyname

## Behavior notes

- In listen mode, destination token can be interpreted as local port when -p is omitted.
- In UDP mode, connection reset behavior is tuned through SIO_UDP_CONNRESET.
- Ctrl+C/console signals trigger socket shutdown/cleanup path.

## Exit behavior

- Returns 0 for successful command execution path.
- Returns non-zero for invalid options, missing destination/port, resolver failures, or socket setup/connect failures.

## Examples

```text
nc www.example.com http
nc -zv -w 2 192.168.1.1 20-80
nc -l -p 4444 -e cmd.exe
nc -u -l -p 5353
```
