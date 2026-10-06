#include "uptime_options.hpp"

void UptimeOptions::printUsage() {
        std::cout << R"(uptime(1)               CrossShell for UNIX Reference Manual                 uptime(1)

    NAME
        uptime - tell how long the system has been running

    SYNOPSIS
        uptime [OPTIONS]

    DESCRIPTION
        Display how long the system has been running, the current time, the
        number of active users, and system load averages.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -p, --pretty
            Show uptime in pretty format.

        -s, --since
            Show when the system started.

        -h, --help
            Display this reference manual and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        uptime
            Display current uptime and system load.

        uptime -p
            Display human-readable uptime.

        uptime -s
            Display system boot time.

    CrossShell for UNIX                                                      uptime(1)
)";
    }

void UptimeOptions::printVersion() {
        std::cout << "uptime 1.0.0\n";
    }

bool UptimeOptions::parse(int argc, char* argv[], UptimeOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                break;
            } else if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                printUsage();
                std::exit(0);
            } else if (arg == "-V" || arg == "--version" || arg == "-v") {
                printVersion();
                std::exit(0);
            } else if (arg == "-p" || arg == "--pretty") {
                opts.pretty = true;
            } else if (arg == "-s" || arg == "--since") {
                opts.since = true;
            } else if (arg[0] == '-') {
                std::cerr << "uptime: invalid option '" << arg << "'\n";
                return false;
            } else {
                std::cerr << "uptime: unexpected argument '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }
