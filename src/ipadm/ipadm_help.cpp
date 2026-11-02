// Help Manual Output
#include "ipadm_help.h"

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
}
