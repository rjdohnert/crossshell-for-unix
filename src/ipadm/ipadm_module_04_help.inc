// Help Manual Output
void PrintIpAdmHelp() {
    std::cout << R"(ipadm(1)                 CrossShell for UNIX Reference Manual                    ipadm(1)

NAME
    ipadm - administer Windows IP interfaces and address objects

SYNOPSIS
    ipadm SUBCOMMAND [OPTIONS] [TARGET]

DESCRIPTION
    Emulates Solaris ipadm using Windows IP Helper APIs to inspect interfaces,
    manage static or DHCP addresses, and adjust protocol properties.

SUBCOMMANDS
    show-if                Display interface state.
    show-addr              Display assigned address objects.
    show-prop              Display IP/TCP properties.
    create-if              Enable the IP stack on an interface.
    delete-if              Disable the IP stack on an interface.
    create-addr            Assign a static or DHCP address.
    delete-addr            Remove an address object.
    set-prop               Modify a protocol property.
    help, -?               Display this reference manual.

OPTIONS
    -T TYPE                Address type: static or dhcp.
    -a ADDRESS/PREFIX      Address and CIDR prefix.
    -p PROPERTY[=VALUE]    Select or set a protocol property.
    --json, --csv, --table Select output format.
    --pipe COMMAND         Send formatted output through COMMAND.

EXAMPLES
    ipadm show-if
    ipadm show-addr
    ipadm show-prop -p ttl ip
    ipadm create-addr -T static -a 192.168.1.50/24 net0/v4static
    ipadm delete-addr net0/v4static

EXIT STATUS
    0          Help or successful interface/address operation.
    1          Invalid command, target, property, or Windows API failure.

CrossShell for UNIX                                                        ipadm(1)
)";
    return;

    std::cout << R"(
IPADM v1.0.0 - IP Interface Administration
Copyright (C) 2026. Roberto J Dohnert, All Rights Reserved.
-------------------------------------------------------------------

USAGE:
  ipadm show-if [ifname]
  ipadm show-addr [addrobj]
  ipadm show-prop [-p prop] [protocol]
  ipadm create-if <ifname>
  ipadm delete-if <ifname>
  ipadm create-addr -T <static|dhcp> -a <addr/prefix> <addrobj>
  ipadm delete-addr <addrobj>
  ipadm set-prop -p <prop=value> <protocol>
  ipadm help, -?

DESCRIPTION:
  Emulates the Oracle Solaris 'ipadm' command on Windows NT network stacks.
  Manages IP interfaces, static/DHCP address assignments, and TCP/IP protocol
  tunables using native Windows IP Helper APIs.

SUBCOMMANDS:
  show-if        Display IP interface status (Class, State, Active).
  show-addr      Display assigned IP address objects, types, and CIDR addresses.
  show-prop      Display tunable IP/TCP protocol parameters (TTL, Forwarding).
  create-if      Enable IP protocol stack on a target network data link.
  delete-if      Disable IP protocol stack on a target network data link.
  create-addr    Assign a static or DHCP IPv4/IPv6 address object to an interface.
  delete-addr    Remove an assigned IP address object from an interface.
  set-prop       Modify a tunable protocol property (e.g., ip forwarding=on).

EXAMPLES:
  ipadm show-if                         Display summary of all IP interfaces.
  ipadm show-addr                       Display all assigned IP address objects.
  ipadm show-prop -p ttl ip             Display Default TTL property for IP.
  ipadm create-addr -T static -a 192.168.1.50/24 net0/v4static
  ipadm delete-addr net0/v4static       Remove assigned static address object.
)";
}
