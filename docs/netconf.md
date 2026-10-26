# netconf

## What it does
Native Windows network port configuration and telemetry reporter in an HP-UX `netconf` / `portconf` style. Enumerates active and unlinked network adapters (Ethernet, Wi-Fi, Virtual switches, Loopback, Bluetooth), operational link states, duplex notations (e.g. `1000FD`, `AUTO`), station addresses (MAC), assigned IPv4/IPv6 addresses, default gateways, DNS servers, and MTU.

## Synopsis
```text
netconf [OPTIONS] [INTERFACE]
```

## Options
- `-h`, `--help`, `/?`: Display command manual and usage examples.
- `-d`, `--detail`: Display detailed adapter telemetry (IPv4, IPv6, Gateway, DNS, MTU).
- `-j`, `--json`: Output records in JSON format (pipable to `jq` or PowerShell).
- `-c`, `--csv`: Output records in Comma-Separated Values (CSV).
- `-a`, `--all`: Report all interfaces including unlinked or disabled `DOWN` ports.
- `-6`, `--ipv6`: Prefer IPv6 addresses in the main `IP-ADDRESS` column.
- `--filters`: Include intermediate NDIS lightweight filter (LWF) pseudo-devices.
- `-v`, `--version`: Print program version.

## Arguments
- `INTERFACE`: Optional port identifier (e.g. `lan0`), adapter friendly name (e.g. `Wi-Fi`), or description substring.

## Examples
```powershell
# Display active network ports
netconf

# Detailed report for lan0
netconf -d lan0

# Filter only Wi-Fi adapter
netconf Wi-Fi

# Inspect via PowerShell JSON pipeline
netconf -j | ConvertFrom-Json | Select-Object port, type, speed_duplex, station_address, gateway

# Export CSV for network inventory
netconf --csv > network_ports.csv
```

## HP-UX origin
Modeled after HP-UX network configuration facility and portconf operational tables.
