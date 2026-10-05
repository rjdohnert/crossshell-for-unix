#include "options.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

void LscfgOptions::PrintVersion() {
    std::cout << "lscfg v1.0.0\n";
    std::cout << "Copyright (C) 2026, Roberto J Dohnert\n";
    std::cout << "Licensed under the BSD 3-Clause License\n";
}

void LscfgOptions::PrintHelp(const char* exeName) {
    std::cout << "\n";
    std::cout << "                           lscfg v1.0.0                   \n";
    std::cout << "\n\n";
    std::cout << "Queries hardware devices, physical slot locations, and Vital Product Data\n";
    std::cout << "(VPD) such as Serial Numbers, Part Numbers, Firmware/Driver levels, and MACs.\n\n";

    std::cout << "USAGE:\n";
    std::cout << "  " << exeName << " [OPTIONS]\n\n";

    std::cout << "OPTIONS:\n";
    std::cout << "  -v, --verbose           Display Vital Product Data (VPD) for installed resources.\n";
    std::cout << "  -s, --summary           Display compact 1-line resource summary output.\n";
    std::cout << "      --table             Display aligned table output (default).\n";
    std::cout << "      --csv               Display CSV output.\n";
    std::cout << "      --json              Display JSON output.\n";
    std::cout << "      -                   Read filters from standard input.\n";
    std::cout << "  -l, --line <class|name> Filter listing by specific device class or resource name.\n";
    std::cout << "                          Supported classes: sys, cpu, mem, disk, net, gpu.\n";
    std::cout << "      --version           Display version information and exit.\n";
    std::cout << "  -h, --help              Display this comprehensive help documentation and exit.\n\n";

    std::cout << "SUPPORTED DEVICE CLASSES (-l):\n";
    std::cout << "  sys    : System Motherboard, BIOS, SMBIOS, and Planar VPD\n";
    std::cout << "  cpu    : Processor Sockets, Cores, Threads, and Clock Speeds\n";
    std::cout << "  mem    : Physical RAM DIMMs, Speeds, Part Numbers, and Serials\n";
    std::cout << "  disk   : Storage Controllers, Hard Drives, and NVMe SSDs\n";
    std::cout << "  net    : Network Interface Cards (NICs) and MAC Addresses\n";
    std::cout << "  gpu    : Video Display Controllers, VRAM, and Driver Versions\n\n";

    std::cout << "AIX OUTPUT LAYOUT FORMAT:\n";
    std::cout << "  INSTALLED RESOURCE LIST\n";
    std::cout << "    RESOURCE         LOCATION            DESCRIPTION\n";
    std::cout << "    sys0             System Planar       Dell Inc. XPS 15 9520\n";
    std::cout << "      Manufacturer...................Dell Inc.\n";
    std::cout << "      System Serial Number...........1234567\n\n";

    std::cout << "EXAMPLES:\n";
    std::cout << "  " << exeName << "                      List all hardware resources (Summary Mode)\n";
    std::cout << "  " << exeName << " -v                   List all hardware resources with detailed VPD\n";
    std::cout << "  " << exeName << " -v -l mem            Display detailed VPD for RAM memory modules only\n";
    std::cout << "  " << exeName << " -v -l hdisk0         Display detailed VPD for primary storage disk\n";
    std::cout << "  " << exeName << " -s -l cpu            Display quick summary of CPU resources\n\n";
}

bool LscfgOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp(argv[0]);
            return false;
        } else if (arg == "--version") {
            PrintVersion();
            return false;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-s" || arg == "--summary") {
            summaryOnly = true;
        } else if (arg == "--table") {
            format = OutputFormat::Table;
        } else if (arg == "--csv") {
            format = OutputFormat::Csv;
        } else if (arg == "--json") {
            format = OutputFormat::Json;
        } else if (arg == "-") {
            std::getline(std::cin, filter, '\0');
            std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        } else if (!arg.empty() && arg[0] != '-') {
            filter = arg;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        } else if ((arg == "-l" || arg == "--line") && i + 1 < argc) {
            filter = argv[++i];
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        }
    }
    return true;
}
