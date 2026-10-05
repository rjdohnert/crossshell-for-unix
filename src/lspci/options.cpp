#include "options.hpp"
#include <iostream>
#include <regex>
#include <algorithm>
#include <cctype>

void CommandLineParser::printHelp() {
    std::cout << R"(lspci(1)                CrossShell for UNIX Reference Manual                 lspci(1)

    NAME
        lspci - list PCI and PCIe devices attached to the system

    SYNOPSIS
        lspci [OPTIONS] [FILE | -]

    DESCRIPTION
        Enumerates present PCI and PCIe devices through the Windows SetupAPI
        subsystem and displays vendor, device, class, driver, subsystem, revision,
        and hardware-path topology. Pipe-delimited inventory can be read from
        standard input and rendered in another supported format.

    OPTIONS
        -v, --verbose
            Display detailed execution diagnostics, hardware paths, subsystem
            identifiers, and revisions.

        -k, --drivers
            Display the Windows kernel driver service for each device.

        -f, --format FORMAT
            Select classic, table, json, csv, or pipeline output. The default
            is classic.

        -s [[[[DOMAIN]:]BUS]:][SLOT][.FUNCTION]
            Show only devices matching the hexadecimal PCI address fields.

        -d [VENDOR]:[DEVICE]
            Show only devices matching hexadecimal vendor and device IDs.

        -
            Read pipe-delimited device data from standard input instead of
            enumerating live hardware.

        -h, --help
            Display this reference manual.

    AVAILABLE MODES
        classic
            Display conventional lspci-style device descriptions.

        table
            Display devices in an aligned text table.

        json
            Display structured JSON for automated processing.

        csv
            Display comma-separated values.

        pipeline
            Display pipe-delimited records suitable for later lspci input.

    EXAMPLES
        lspci
            List present PCI devices using the classic layout.

        lspci -k -v
            Include driver services and detailed hardware information.

        lspci -s 00:08.0
            List devices at bus 00, slot 08, function 0.

        lspci -d 103c:
            List all devices from vendor 103c.

        lspci -f json | ConvertFrom-Json
            Process the inventory as JSON in PowerShell.

        lspci -f pipeline > pci.txt
        lspci - -f table < pci.txt
            Save device records and render them later as a table.

    CrossShell for UNIX                                                    lspci(1)
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
        } else if (arg == "-") {
            opts.readStdin = true;
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            opts.format = argv[++i];
        } else if (arg == "-s" && i + 1 < argc) {
            std::string spec = argv[++i];
            std::regex sRegex(R"(^(?:(?:([0-9a-fA-F]+):)?([0-9a-fA-F]+):)?([0-9a-fA-F]+)?(?:\.([0-9a-fA-F]+))?$)");
            std::smatch m;
            if (std::regex_match(spec, m, sRegex)) {
                if (m[1].matched) opts.filterDom  = std::stoi(m[1].str(), nullptr, 16);
                if (m[2].matched) opts.filterBus  = std::stoi(m[2].str(), nullptr, 16);
                if (m[3].matched) opts.filterSlot = std::stoi(m[3].str(), nullptr, 16);
                if (m[4].matched) opts.filterFunc = std::stoi(m[4].str(), nullptr, 16);
            }
        } else if (arg == "-d" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto col = spec.find(':');
            if (col != std::string::npos) {
                opts.filterVid = spec.substr(0, col);
                opts.filterDid = spec.substr(col + 1);
            } else {
                opts.filterVid = spec;
            }
            std::transform(opts.filterVid.begin(), opts.filterVid.end(), opts.filterVid.begin(), ::tolower);
            std::transform(opts.filterDid.begin(), opts.filterDid.end(), opts.filterDid.begin(), ::tolower);
        }
    }
    return opts;
}
