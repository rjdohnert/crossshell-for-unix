#include "options.hpp"
#include <iostream>
#include <cstdlib>

void CommandLineParser::printHelp() {
    std::cout << R"(lsbt(1)                 CrossShell for UNIX Reference Manual                  lsbt(1)

NAME
    lsbt - list Bluetooth radios, paired devices, and peripherals

SYNOPSIS
    lsbt [OPTIONS] [FILE | -]

DESCRIPTION
    Enumerates Bluetooth radios, paired peripherals, Class of Device (CoD),
    and active connections using Windows Bluetooth APIs and SetupAPI subsystem.
    Pipe-delimited inventory can be read from standard input and rendered in
    another supported format.

OPTIONS
    -v, --verbose
        Display verbose details including hardware paths, Class of Device
        (CoD), and connection states.

    -k, --drivers
        Show Windows Bluetooth profile drivers in use (e.g., BthA2DP).

    -c, --connected
        Filter and display only currently connected Bluetooth devices.

    -f, --format FORMAT
        Select classic, table, json, csv, or pipeline output layout.
        The default is classic.

    -s [[BUS]:][SLOT]
        Filter listing by simulated bus/slot index in hexadecimal.

    -m MAC_PREFIX
        Filter listing by MAC address prefix (e.g., a4:83 or a483).

    -
        Read pipe-delimited Bluetooth stream from standard input instead of
        querying live radios.

    -h, --help
        Display this reference manual.

AVAILABLE MODES
    classic
        Standard format with status and device name.

    table
        Aligned visual ASCII border table.

    json
        Structured JSON for scripts and jq pipelines.

    csv
        RFC-4180 standard comma-separated values.

    pipeline
        Pipe-delimited stream for PowerShell and CMD processing.

EXAMPLES
    lsbt
        Default standard listing of paired Bluetooth devices.

    lsbt -f table
        Print aligned ASCII table of Bluetooth devices.

    lsbt -c -v
        Show currently connected peripherals in verbose mode.

    lsbt -f csv > bt_devices.csv
        Export paired devices directly to CSV.

    lsbt -k
        Inspect active driver modules.

    lsbt -f json | ConvertFrom-Json | Where-Object connected -eq $true
        Query connected Bluetooth devices with PowerShell and JSON.

    lsbt -f pipeline | findstr "Audio/Video"
        Stream processing with findstr.

    type bt_dump.txt | lsbt - -f table
        Ingest piped input from STDIN and render as an ASCII table.

CrossShell for UNIX                                                     lsbt(1)
)";
}

CommandLineOptions CommandLineParser::parse(int argc, char* argv[]) {
    CommandLineOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            std::exit(0);
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-k" || arg == "--drivers") {
            opts.showDrivers = true;
        } else if (arg == "-c" || arg == "--connected") {
            opts.onlyConnected = true;
        } else if (arg == "-") {
            opts.readStdin = true;
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            opts.format = argv[++i];
        } else if (arg == "-s" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto col = spec.find(':');
            if (col != std::string::npos) {
                if (col > 0) opts.filterBus = std::stoi(spec.substr(0, col), nullptr, 16);
                if (col + 1 < spec.size()) opts.filterSlot = std::stoi(spec.substr(col + 1), nullptr, 16);
            } else {
                opts.filterSlot = std::stoi(spec, nullptr, 16);
            }
        } else if (arg == "-m" && i + 1 < argc) {
            opts.filterMac = argv[++i];
        }
    }
    return opts;
}
