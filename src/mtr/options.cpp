#include "options.hpp"

void CliParser::displayHelp(const char* progName) {
    (void)progName;
    std::cout << R"(mtr(1)                  CrossShell for UNIX Reference Manual                   mtr(1)

    NAME
        mtr - network diagnostic and traceroute tool (My Traceroute)

    SYNOPSIS
        mtr [OPTIONS] HOSTNAME_OR_IP

    DESCRIPTION
        mtr combines the functionality of traceroute and ping into a single
        network diagnostic tool. As mtr starts, it investigates the network
        connection between the host mtr runs on and a user-specified destination
        host.

    OPTIONS
        -c, --report-cycles COUNT
            Send COUNT pings to each hop, then print report and exit.

        -m, --max-ttl HOPS
            Set maximum hops / Time-To-Live (default: 30, max: 255).

        -i, --interval SECONDS
            Set interval between ping cycles (0.01-86400, default: 1.0).

        -t, --timeout MS
            ICMP reply timeout in milliseconds (1-60000, default: 1000).

        -s, --psize BYTES
            ICMP payload size in bytes (0-65500, default: 32).

        -n, --no-dns
            Do not resolve hostnames (display IP addresses only).

        -h, --help
            Display this reference manual.

        -v, --version
            Output version information and exit.

    INTERACTIVE COMMANDS
        q, Q
            Quit MTR.

        r, R
            Reset statistical counters.

        n, N
            Toggle reverse DNS name resolution.

        p, P
            Pause / resume dynamic probing.

    EXAMPLES
        mtr google.com
            Start interactive real-time trace to google.com.

        mtr -n -c 10 1.1.1.1
            Send 10 ping cycles without DNS resolution and print report.

        mtr --interval 0.5 --max-ttl 15 cloudflare.com
            Trace route with 500ms interval up to 15 hops.

    CrossShell for UNIX                                                    mtr(1)
)";
}

void CliParser::displayVersion() {
    std::cout << "MTR v1.2.0\n";
}

bool CliParser::parseInteger(const char* text, long long minimum, long long maximum, long long& value) {
    try {
        size_t consumed = 0;
        long long parsed = std::stoll(text, &consumed, 10);
        if (consumed != std::string(text).size() || parsed < minimum || parsed > maximum) {
            return false;
        }
        value = parsed;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CliParser::parseInterval(const char* text, double& value) {
    try {
        size_t consumed = 0;
        double parsed = std::stod(text, &consumed);
        if (consumed != std::string(text).size() || !std::isfinite(parsed) ||
            parsed < 0.01 || parsed > 86400.0) {
            return false;
        }
        value = parsed;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::unique_ptr<MtrOptions> CliParser::parse(int argc, char* argv[]) {
    if (argc < 2) {
        return nullptr;
    }

    auto opts = std::make_unique<MtrOptions>();
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            opts->showHelp = true;
            return opts;
        } else if (arg == "-v" || arg == "--version") {
            opts->showVersion = true;
            return opts;
        } else if (arg == "-n" || arg == "--no-dns") {
            opts->resolveDns = false;
        } else if (arg == "-c" || arg == "--report-cycles") {
            long long value = 0;
            if (i + 1 >= argc || !parseInteger(argv[++i], 1, (std::numeric_limits<int>::max)(), value)) {
                std::cerr << "MTR: Invalid report cycle count\n";
                return nullptr;
            }
            opts->reportCycles = static_cast<int>(value);
        } else if (arg == "-m" || arg == "--max-ttl") {
            long long value = 0;
            if (i + 1 >= argc || !parseInteger(argv[++i], 1, 255, value)) {
                std::cerr << "MTR: Maximum TTL must be between 1 and 255\n";
                return nullptr;
            }
            opts->maxTtl = static_cast<uint8_t>(value);
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 >= argc || !parseInterval(argv[++i], opts->intervalSec)) {
                std::cerr << "MTR: Interval must be a finite value between 0.01 and 86400 seconds\n";
                return nullptr;
            }
        } else if (arg == "-t" || arg == "--timeout") {
            long long value = 0;
            if (i + 1 >= argc || !parseInteger(argv[++i], 1, 60000, value)) {
                std::cerr << "MTR: Timeout must be between 1 and 60000 milliseconds\n";
                return nullptr;
            }
            opts->timeoutMs = static_cast<DWORD>(value);
        } else if (arg == "-s" || arg == "--psize") {
            long long value = 0;
            if (i + 1 >= argc || !parseInteger(argv[++i], 0, 65500, value)) {
                std::cerr << "MTR: Packet size must be between 0 and 65500 bytes\n";
                return nullptr;
            }
            opts->packetSize = static_cast<size_t>(value);
        } else if (arg[0] != '-') {
            opts->target = std::string(arg);
        } else {
            std::cerr << "MTR: Unknown parameter: " << arg << "\n";
            return nullptr;
        }
    }

    if (opts->target.empty()) {
        std::cerr << "MTR: Target hostname or IP address is required.\n";
        return nullptr;
    }

    return opts;
}
