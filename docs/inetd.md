# inetd

## What it does
Acts as a super-server that listens on configured TCP and UDP ports and starts the appropriate service handler when traffic arrives.

It supports classic `inetd.conf`-style service definitions and can also run built-in micro-services such as echo, daytime, and discard.

## Usage
```text
inetd [options]
```

## Options
- `-c`, `--config <file>`: select the configuration file to load
- `-d`, `--debug`: run in debug foreground mode
- `-v`, `--verbose`: enable verbose logging
- `-h`, `--help`: show help text

## Configuration Format
```text
<service_name> <sock_type> <proto> <wait/nowait> <user> <server_prog> [args...]
```

Fields:
- `service_name`: service name or numeric port
- `sock_type`: `stream` or `dgram`
- `proto`: `tcp` or `udp`
- `wait/nowait`: whether the server holds the socket or accepts new connections concurrently
- `user`: compatibility field preserved for Solaris-style configurations
- `server_prog`: executable path or `internal`
- `args`: optional command-line arguments

## Built-in Services
- `echo`: reflects received data back to the client
- `daytime`: returns the current date and time
- `discard`: consumes input and returns nothing

## Examples
- `inetd`
- `inetd -d`
- `inetd -c C:\Services\inetd.conf -v`

Example configuration lines:
```text
echo       stream  tcp  nowait  root  internal
daytime    dgram   udp  wait    root  internal
sys_info   stream  tcp  nowait  root  C:\Windows\System32\hostname.exe
```

## Notes
- Administrator privileges are required.
- The daemon binds to TCP/UDP services and dispatches work based on the configuration file.
- `internal` services are handled by the program itself rather than an external executable.

## UNIX origin
A classic Unix super-server from traditional `inetd`-based systems.