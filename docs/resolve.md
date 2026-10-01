# resolve

## What it does
Queries DNS name servers interactively or in one-shot mode.

This command behaves like a lightweight `nslookup`-style tool and supports record-type selection, custom DNS servers, reverse lookups, and an interactive prompt.

## Usage
```text
resolve [-option ...] [host-to-find | - [server]]
```

## Options
- `-type=TYPE`: set the DNS record type to query
- `-querytype=TYPE`: synonym for `-type`
- `-debug`: enable verbose DNS debug output
- `-nodebug`: disable verbose debug output
- `-?`, `-h`, `--help`: show help text

## Supported Query Types
- `A`
- `AAAA`
- `MX`
- `NS`
- `PTR`
- `SOA`
- `TXT`
- `ANY`

## Interactive Mode Commands
- `<host|ip>`: look up a hostname or IP address
- `server <ip|name>`: change the default DNS server
- `set type=TYPE`: change the query type
- `set debug`: enable debug logging
- `set nodebug`: disable debug logging
- `help` or `?`: show interactive help
- `exit` or `quit`: leave interactive mode

## Examples
- `resolve www.example.com`
- `resolve -type=MX example.com dns.google`
- `resolve 8.8.8.8`
- `resolve`

## Notes
- The tool auto-detects PTR lookups when you pass an IP address.
- You can point it at a DNS server by IP address or hostname.
- Interactive mode is useful for repeated queries without restarting the command.

## UNIX origin
A compatibility-style DNS lookup utility inspired by the classic `nslookup` workflow from Unix and Unix-like systems.