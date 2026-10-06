#include "options.hpp"

bool PortmapOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            showHelp = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            showVersion = true;
            return true;
        } else if (arg == "-a" || arg == "--all") {
            showAll = true;
        } else if (arg == "-t" || arg == "--tcp") {
            proto = ProtocolFilter::TCP_ONLY;
        } else if (arg == "-u" || arg == "--udp") {
            proto = ProtocolFilter::UDP_ONLY;
        } else if (arg == "-4" || arg == "--ipv4") {
            ipVer = IpFilter::IPV4_ONLY;
        } else if (arg == "-6" || arg == "--ipv6") {
            ipVer = IpFilter::IPV6_ONLY;
        } else if (arg == "-n" || arg == "--no-proc") {
            showProcess = false;
        } else if ((arg == "-s" || arg == "--state") && i + 1 < argc) {
            std::string state = argv[++i];
            std::transform(state.begin(), state.end(), state.begin(), ::toupper);
            stateFilter = state;
        } else {
            std::cerr << "portmap: unknown option '" << arg << "'\n";
            return false;
        }
    }
    return true;
}

void PortmapOptions::PrintHelp() const {
    std::cout << R"(portmap(1)              CrossShell for UNIX Reference Manual               portmap(1)

    NAME
        portmap - display RPC and active TCP/UDP listening port mappings

    SYNOPSIS
        portmap [OPTIONS]

    DESCRIPTION
        portmap queries and displays all active listening network endpoints,
        bound RPC programs, and associated owning process names.

    OPTIONS
        -a, --all
            Display all endpoints including established connections.

        -t, --tcp
            Filter to TCP endpoints only.

        -u, --udp
            Filter to UDP endpoints only.

        -4, --ipv4
            Filter to IPv4 endpoints.

        -6, --ipv6
            Filter to IPv6 endpoints.

        -n, --no-proc
            Do not resolve process names/PIDs.

        -s, --state STATE
            Filter by state (e.g. LISTENING, ESTABLISHED).

        --json, --csv, --table
            Format port map records as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        portmap -t -s LISTENING
            List all listening TCP ports.

    CrossShell for UNIX                                                portmap(1)
)";
}

void PortmapOptions::PrintVersion() const {
    std::cout << "portmap 2.6.0\n";
}
