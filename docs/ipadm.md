# ipadm

## What it does

Reports and manages IP interfaces and address objects in a Solaris `ipadm`-style layout for Windows. It exposes a simplified view of interface status, addresses, and protocol properties using the Windows IP Helper APIs.

## Usage

- `ipadm.exe show-if [ifname]`
- `ipadm.exe show-addr [addrobj]`
- `ipadm.exe show-prop [-p prop] [protocol]`
- `ipadm.exe create-if <ifname>`
- `ipadm.exe delete-if <ifname>`
- `ipadm.exe create-addr -T <static|dhcp> -a <addr/prefix> <addrobj>`
- `ipadm.exe delete-addr <addrobj>`
- `ipadm.exe set-prop -p <prop=value> <protocol>`
- `ipadm.exe help`

## Subcommands

- `show-if`: list interfaces and their state as Solaris-style `IFNAME`, `CLASS`, `STATE`, `ACTIVE`
- `show-addr`: list address objects and IPv4/IPv6 CIDR assignments
- `show-prop`: display protocol properties such as forwarding and TTL for IP
- `create-addr`: create a static or DHCP-style address object on a selected interface
- `delete-addr`: remove a matching address object

## Notes

- This is a Windows-native approximation of the Solaris `ipadm` command rather than a complete implementation of the Solaris networking stack.
- The interface list is filtered to present real networking interfaces and the loopback interface while suppressing synthetic Windows-only adapters.
- The actual Windows APIs do not expose a native Solaris IP object model, so behavior is intentionally emulated.
- IPv4/v6 address reporting and creation are limited to the subset supported by the Windows IP Helper and unicast address APIs.

## Examples

- `ipadm.exe show-if`
- `ipadm.exe show-addr`
- `ipadm.exe show-prop -p forwarding ip`
- `ipadm.exe create-addr -T static -a 192.168.1.50/24 net0/v4static`
- `ipadm.exe delete-addr net0/v4static`

## UNIX origin

The original `ipadm` utility comes from Oracle Solaris and is used to manage IP interfaces, addresses, and protocol properties.
