#include "whois_options.hpp"

bool WhoisOptions::Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    target = argv[i] ? argv[i] : "";
                }
                break;
            } else if (arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                showVersion = true;
                return true;
            } else if ((arg == "-H" || arg == "--host") && i + 1 < argc) {
                server = argv[++i];
            } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
                port = argv[++i];
            } else if (arg == "-a") {
                server = "whois.arin.net";
            } else if (arg == "-A") {
                server = "whois.apnic.net";
            } else if (arg == "-c") {
                server = "whois.lacnic.net";
            } else if (arg == "-r") {
                server = "whois.ripe.net";
            } else if (arg == "-d") {
                server = "whois.radb.net";
            } else if (arg == "-I") {
                server = "whois.iana.org";
            } else if (arg == "-f" || arg == "--no-follow") {
                autoFollow = false;
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "whois: unknown option -- " << arg << "\n";
                return false;
            } else {
                target = arg;
            }
        }

        if (target.empty() && !showHelp && !showVersion) {
            return false;
        }

        if (server.empty()) {
            server = "whois.iana.org";
        }

        return true;
    }

void WhoisOptions::PrintUsage(const char* progName) const {
        std::cout << R"(whois(1)                CrossShell for UNIX Reference Manual                 whois(1)

    NAME
        whois - client for the WHOIS directory service

    SYNOPSIS
        whois [OPTIONS] OBJECT

    DESCRIPTION
        whois searches for an object in a RFC 3912 WHOIS database server.

    OPTIONS
        -H, --host HOST
            Connect to server HOST.

        -p, --port PORT
            Connect to port PORT (default 43).

        -a
            Query ARIN database.

        -A
            Query APNIC database.

        -r
            Query RIPE database.

        --json, --csv, --table
            Output WHOIS response records as JSON, CSV, or table.

        --pipe COMMAND
            Send query output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        whois example.com
            Query WHOIS database for example.com.

    CrossShell for UNIX                                                  whois(1)
)";
    }

void WhoisOptions::PrintVersion() const {
        std::cout << "whois 1.0.0\n";
    }
