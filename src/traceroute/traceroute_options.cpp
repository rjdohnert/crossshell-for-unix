#include "traceroute_options.hpp"

bool TracerouteOptions::Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-?" || arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-n") {
                numericMode = true;
            } else if (arg == "-I") {
                icmpMode = true;
            } else if (arg == "-m" && i + 1 < argc) {
                maxTtl = std::atoi(argv[++i]);
            } else if (arg == "-f" && i + 1 < argc) {
                firstTtl = std::atoi(argv[++i]);
            } else if (arg == "-p" && i + 1 < argc) {
                basePort = std::atoi(argv[++i]);
            } else if (arg == "-q" && i + 1 < argc) {
                nQueries = std::atoi(argv[++i]);
            } else if (arg == "-w" && i + 1 < argc) {
                waitTimeSec = std::atoi(argv[++i]);
            } else if (arg == "-s" && i + 1 < argc) {
                srcIpStr = argv[++i];
            } else if (!arg.empty() && arg[0] != '-') {
                targetHostStr = arg;
            } else {
                std::cerr << "traceroute: unknown option '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }

void TracerouteOptions::PrintHelp(const char* progName) const {
        std::cout << R"(traceroute(1)           CrossShell for UNIX Reference Manual           traceroute(1)

    NAME
        traceroute - print the route packets trace to network host

    SYNOPSIS
        traceroute [OPTIONS] HOST

    DESCRIPTION
        traceroute tracks the route packets follow across an IP network on their
        way to a given host using ICMP or UDP probe packets with incrementing TTL.

    OPTIONS
        -I
            Use ICMP ECHO for probes instead of UDP.

        -n
            Do not resolve IP addresses to hostnames.

        -m MAX_TTL
            Set maximum number of hops (TTL value) to probe.

        -w SECONDS
            Set wait time for probe response.

        -p PORT
            Set base destination port for UDP probes.

        -q NQUERIES
            Set number of probe packets per hop.

        --json, --csv, --table
            Output hop records as JSON, CSV, or table.

        --pipe COMMAND
            Stream hop telemetry to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        traceroute -I google.com
            Trace route using ICMP ECHO probes.

    CrossShell for UNIX                                            traceroute(1)
)";
    }

void TracerouteOptions::PrintVersion() const {
        std::cout << "traceroute 1.0.0\n";
    }
