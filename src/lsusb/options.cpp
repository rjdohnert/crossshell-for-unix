#include "options.hpp"
#include <iostream>
#include <cstdlib>

void CommandLineParser::printHelp() {
    std::cout << R"(lsusb(1)                CrossShell for UNIX Reference Manual                 lsusb(1)

NAME
    lsusb - list USB devices and topology attached to the system

SYNOPSIS
    lsusb [OPTIONS] [FILE | -]

DESCRIPTION
    Enumerates present USB host controllers, hubs, peripherals, and composite
    devices using the Windows SetupAPI subsystem. Supports filtering by bus,
    device address, vendor ID, and product ID. Pipe-delimited inventory can be
    read from standard input and rendered in another supported format.

OPTIONS
    -v, --verbose
        Display verbose details including device classes, hardware paths,
        and serial numbers.

    -f, --format FORMAT
        Select classic, table, json, csv, or pipeline output layout.
        The default is classic.

    -s [[BUS]:][DEVNUM]
        Filter device listing by decimal bus and/or device address.

    -d [VENDOR]:[PRODUCT]
        Filter device listing by hexadecimal vendor and/or product ID.

    -
        Read pipe-delimited device data from standard input instead of
        enumerating live hardware.

    -h, --help
        Display this reference manual.

AVAILABLE MODES
    classic
        Standard HP-UX / Linux lsusb format.

    table
        Formatted ASCII grid layout.

    json
        Structured JSON for programmatic tooling and jq.

    csv
        RFC-4180 standard comma-separated values.

    pipeline
        Pipe-delimited stream for PowerShell and CMD processing.

EXAMPLES
    lsusb
        List present USB devices using the classic format.

    lsusb -f table
        Display connected USB devices in an aligned ASCII table.

    lsusb -d 03f0: -v
        Filter devices by vendor prefix (03f0) in verbose mode.

    lsusb -f csv > usb_inventory.csv
        Export device inventory directly to CSV.

    lsusb -f json | ConvertFrom-Json | Select-Object id, description
        Process USB inventory as JSON in PowerShell.

    lsusb -f pipeline | findstr "Mass Storage"
        Filter pipeline output stream with findstr.

    type devices.txt | lsusb - -f table
        Ingest piped data from STDIN and render as an ASCII table.

CrossShell for UNIX                                                    lsusb(1)
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
        } else if (arg == "-") {
            opts.readStdin = true;
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            opts.format = argv[++i];
        } else if (arg == "-s" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto col = spec.find(':');
            if (col != std::string::npos) {
                if (col > 0) opts.filterBus = std::stoi(spec.substr(0, col));
                if (col + 1 < spec.size()) opts.filterDev = std::stoi(spec.substr(col + 1));
            } else {
                opts.filterBus = std::stoi(spec);
            }
        } else if (arg == "-d" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto col = spec.find(':');
            if (col != std::string::npos) {
                opts.filterVid = spec.substr(0, col);
                opts.filterPid = spec.substr(col + 1);
            } else {
                opts.filterVid = spec;
            }
        }
    }
    return opts;
}
