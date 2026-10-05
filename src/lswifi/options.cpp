#include "options.hpp"
#include <iostream>
#include <io.h>

void RuntimeConfig::PrintHelp() {
    std::cout << R"(lswifi(1)                CrossShell for UNIX Reference Manual                 lswifi(1)

NAME
    lswifi - lists Wi-Fi adapters and nearby wireless access points

SYNOPSIS
    lswifi [OPTIONS]

DESCRIPTION
    Lists Wi-Fi adapters and nearby access points, including connection state,
    SSID, BSSID, signal strength, band, channel, and link quality metrics.
    Provides standard CrossShell semantics and integrates natively with
    Windows console pipelines, standard streams, and file paths.

OPTIONS
    -a, --adapters-only
        Display Wi-Fi adapters and connected status only.

    -n, --networks-only
        Display visible access points and networks only.

    -s, --scan
        Trigger an active Wi-Fi hardware scan before reading.

    -f, --format <format>
        Select output format: table (default), json, or csv.

    --min-signal <quality>
        Filter out networks weaker than the specified quality percentage (0-100).

    --ssid <filter>
        Filter networks matching substring (case-sensitive).

    --band <band>
        Filter networks by band ('2.4', '5', or '6').

    --no-color
        Force-disable ANSI terminal color escapes.

    -h, --help
        Display this comprehensive reference manual and exit.

    -V, --version
        Display version information and exit.

EXAMPLES
    lswifi
        List all Wi-Fi adapters and visible access points in table format.

    lswifi -s --format json
        Trigger an active scan and output results in JSON format.

    lswifi --min-signal 60 --band 5
        Display 5 GHz networks with at least 60% signal quality.

    lswifi --format csv > scan_results.csv
        Export scan results to a CSV file.

EXIT STATUS
    0
        Success.
    1
        Error communicating with the WLAN service or invalid arguments.

CrossShell for UNIX                                                    lswifi(1)
)";
}

void RuntimeConfig::PrintVersion() {
    std::cout << "lswifi 1.0.0\n";
}

bool RuntimeConfig::Parse(int argc, char* argv[]) {
    // Auto-detect redirection / pipes to turn off ANSI formatting
    if (!_isatty(_fileno(stdout))) {
        useColor = false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            return false;
        } else if (arg == "-V" || arg == "--version") {
            PrintVersion();
            return false;
        } else if (arg == "-f" || arg == "--format") {
            if (++i < argc) {
                std::string fmt = argv[i];
                if (fmt == "json") format = RuntimeConfig::Format::Json;
                else if (fmt == "csv") format = RuntimeConfig::Format::Csv;
                else format = RuntimeConfig::Format::Table;
            }
        } else if (arg == "-s" || arg == "--scan") {
            triggerScan = true;
        } else if (arg == "-a" || arg == "--adapters-only") {
            showAdapters = true;
            showNetworks = false;
        } else if (arg == "-n" || arg == "--networks-only") {
            showAdapters = false;
            showNetworks = true;
        } else if (arg == "--min-signal") {
            if (++i < argc) minSignal = std::stoi(argv[i]);
        } else if (arg == "--ssid") {
            if (++i < argc) ssidFilter = argv[i];
        } else if (arg == "--band") {
            if (++i < argc) bandFilter = argv[i];
        } else if (arg == "--no-color") {
            useColor = false;
        }
    }
    return true;
}
