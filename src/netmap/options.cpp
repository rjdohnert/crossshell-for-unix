#include "options.hpp"
#include "engine.hpp"

namespace Netmap {

void HelpSystem::ShowVersion() {
    std::cout << "netmap 1.2.0\n"
              << "\n"
              << "Copyright (C) 2026. All rights reserved.\n";
}

void HelpSystem::ShowHelp(const std::string& section) {
    if (section == "add" || section == "-s") {
        ShowAddHelp();
        return;
    } else if (section == "delete" || section == "-d") {
        ShowDeleteHelp();
        return;
    } else if (section == "scan" || section == "-scan") {
        ShowScanHelp();
        return;
    }

    std::cout << R"(netmap(1)               CrossShell for UNIX Reference Manual                  netmap(1)

    NAME
        netmap - ARP cache management and network mapping utility

    SYNOPSIS
        netmap [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        netmap provides high-performance ARP table inspection, address
        resolution management, and active local network discovery sweeps.
        It interfaces directly with the Windows IP Helper and Winsock APIs.

    OPTIONS
        -a, -g [INET_ADDR]
            Display current ARP table entries, optionally filtered by IP address.

        -s INET_ADDR ETH_ADDR
            Add a static ARP binding mapping INET_ADDR to physical ETH_ADDR.

        -d INET_ADDR
            Delete the ARP entry associated with INET_ADDR (use * for all).

        -f, --flush
            Flush the entire ARP cache across all network interfaces.

        --scan CIDR_OR_RANGE
            Perform an active multi-threaded ARP discovery sweep across a subnet.

        -if, --interfaces
            List all active network interfaces with IDs and addresses.

        -N IF_ADDR_OR_INDEX
            Specify target interface by its IP address or interface index.

        -v, --verbose
            Display unresolvable, stale, and probe-state entries.

        -t, --timeout MS
            Set timeout for active discovery scans in milliseconds (default: 800).

        --threads NUM
            Number of concurrent threads for active subnet scanning (default: 32).

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        netmap -a
            Display complete ARP table across all interfaces.

        netmap -a 192.168.1.1
            Look up ARP entry specifically for gateway 192.168.1.1.

        netmap -s 192.168.1.250 00-11-22-33-44-55
            Add static IP-to-MAC hardware binding.

        netmap --scan 192.168.1.0/24
            Discover all live devices on the local subnet via ARP sweep.

        netmap --flush
            Flush the ARP cache for all network adapters.

    CrossShell for UNIX                                                    netmap(1)
)";
}

void HelpSystem::ShowAddHelp() {
    std::cout << "NETMAP - Static Entry Creation Help (-s)\n\n"
              << "SYNOPSIS:\n"
              << "    netmap -s <inet_addr> <eth_addr> [-N <if_addr|if_index>]\n\n"
              << "DESCRIPTION:\n"
              << "    Creates a permanent, static binding in the ARP cache between an IPv4 address\n"
              << "    and a physical MAC address. If no interface is specified with -N, netmap\n"
              << "    automatically calculates the best interface route via Windows Routing Engine.\n\n"
              << "MAC FORMATS ACCEPTED:\n"
              << "    00-1A-2B-3C-4D-5E   (Standard Windows dash-separated)\n"
              << "    00:1A:2B:3C:4D:5E   (UNIX colon-separated)\n"
              << "    001A2B3C4D5E         (Continuous hexadecimal string)\n\n"
              << "EXAMPLE:\n"
              << "    netmap -s 192.168.1.100 a4-83-e7-21-99-0a\n"
              << "    netmap -s 10.0.0.254 00:50:56:c0:00:08 -N 10.0.0.15\n";
}

void HelpSystem::ShowDeleteHelp() {
    std::cout << "NETMAP - Entry Deletion Help (-d, -f)\n\n"
              << "SYNOPSIS:\n"
              << "    netmap -d <inet_addr|*> [-N <if_addr|if_index>]\n"
              << "    netmap -f | --flush [-N <if_addr|if_index>]\n\n"
              << "DESCRIPTION:\n"
              << "    Removes dynamic or static mapping entries from the system neighbor cache.\n"
              << "    Specifying '*' as the address or running -f will purge the table completely.\n\n"
              << "EXAMPLES:\n"
              << "    netmap -d 192.168.1.100\n"
              << "    netmap -d * -N 192.168.1.50\n"
              << "    netmap --flush\n";
}

void HelpSystem::ShowScanHelp() {
    std::cout << "NETMAP - Active ARP Scanner Help (-scan)\n\n"
              << "SYNOPSIS:\n"
              << "    netmap -scan <cidr_block | ip_range | single_ip> [-t <timeout_ms>] [-threads <count>]\n\n"
              << "DESCRIPTION:\n"
              << "    Directly issues raw ARP request packets using the SendARP kernel pipeline to\n"
              << "    detect active physical hosts on the local Ethernet / Wi-Fi layer. Bypasses\n"
              << "    ICMP firewalls that block standard ping packets.\n\n"
              << "EXAMPLES:\n"
              << "    netmap -scan 192.168.1.0/24\n"
              << "    netmap -scan 192.168.1.1-192.168.1.50 -threads 16\n"
              << "    netmap -scan 10.0.0.1 -t 1500\n";
}

void HelpSystem::ShowInterfaces() {
    auto ifaces = InterfaceManager::GetAllInterfaces();
    std::cout << "\nAvailable Network Interfaces (" << ifaces.size() << " detected):\n";
    std::cout << std::string(75, '=') << "\n";

    for (const auto& iface : ifaces) {
        std::cout << "Index: " << iface.index << " (0x" << std::hex << iface.index << std::dec << ")\n"
                  << "  Friendly Name : " << iface.friendlyName << "\n"
                  << "  Description   : " << iface.description << "\n"
                  << "  MAC Address   : " << iface.macAddress << "\n"
                  << "  Status        : " << (iface.status == IfOperStatusUp ? "UP / ACTIVE" : "DOWN") << "\n"
                  << "  MTU           : " << iface.mtu << "\n"
                  << "  IPv4 Addrs    : ";
        if (iface.ipv4Addresses.empty()) {
            std::cout << "[None Assigned]\n";
        } else {
            for (size_t i = 0; i < iface.ipv4Addresses.size(); ++i) {
                std::cout << iface.ipv4Addresses[i] << (i + 1 < iface.ipv4Addresses.size() ? ", " : "");
            }
            std::cout << "\n";
        }
        std::cout << std::string(75, '-') << "\n";
    }
}

NetmapOptions NetmapOptionsParser::Parse(int argc, char* argv[]) {
    NetmapOptions opts;

    if (argc < 2) {
        opts.mode = Mode::Display;
        return opts;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-a" || arg == "-g") {
            opts.mode = Mode::Display;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts.targetIp = argv[++i];
            }
        } else if (arg == "-s") {
            opts.mode = Mode::Add;
            if (i + 2 < argc) {
                opts.targetIp = argv[++i];
                opts.targetMac = argv[++i];
            } else {
                opts.valid = false;
                opts.errorMessage = "-s requires both <inet_addr> and <eth_addr>. Use 'netmap -h add' for syntax assistance.";
                return opts;
            }
        } else if (arg == "-d") {
            opts.mode = Mode::Delete;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts.targetIp = argv[++i];
            } else {
                opts.targetIp = "*";
            }
        } else if (arg == "-f" || arg == "--flush") {
            opts.mode = Mode::Flush;
        } else if (arg == "-scan" || arg == "--scan") {
            opts.mode = Mode::Scan;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts.scanTarget = argv[++i];
            } else {
                opts.valid = false;
                opts.errorMessage = "-scan requires a subnet CIDR (e.g. 192.168.1.0/24) or range.";
                return opts;
            }
        } else if (arg == "-if" || arg == "--interfaces") {
            opts.mode = Mode::ListInterfaces;
        } else if (arg == "-N") {
            if (i + 1 < argc) {
                opts.ifFilter = argv[++i];
            } else {
                opts.valid = false;
                opts.errorMessage = "-N requires an interface IP address or index.";
                return opts;
            }
        } else if (arg == "-v" || arg == "-verbose") {
            opts.verbose = true;
        } else if (arg == "-V" || arg == "--version") {
            opts.mode = Mode::Version;
        } else if (arg == "-h" || arg == "--help" || arg == "/?") {
            opts.mode = Mode::Help;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                opts.helpTopic = argv[++i];
            }
        } else if (arg == "-t" || arg == "--timeout") {
            if (i + 1 < argc) {
                opts.timeoutMs = std::stoi(argv[++i]);
            }
        } else if (arg == "-threads") {
            if (i + 1 < argc) {
                opts.threadCount = std::stoi(argv[++i]);
            }
        } else if (opts.targetIp == std::nullopt && IsValidIpv4(arg)) {
            opts.targetIp = arg;
        } else {
            opts.valid = false;
            opts.errorMessage = "Unrecognized option: " + arg + "\n    Use 'netmap --help' to display available syntax and modes.";
            return opts;
        }
    }

    return opts;
}

} // namespace Netmap
