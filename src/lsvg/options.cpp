#include "options.hpp"
#include <iostream>

bool CmdOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "-help" || arg == "/?" || arg == "--help") {
            PrintHelp();
            return false; // signals exit(0)
        } else if (arg == "-V" || arg == "--version") {
            PrintVersion();
            return false; // signals exit(0)
        } else if (arg == "-o") {
            listActiveOnly = true;
        } else if (arg == "-l") {
            listLVs = true;
        } else if (arg == "-p") {
            listPVs = true;
        } else if (arg == "-a") {
            listAllDetail = true;
        } else if (arg == "--table") {
            format = LsvgFormat::Table;
        } else if (arg == "--csv") {
            format = LsvgFormat::Csv;
        } else if (arg == "--json") {
            format = LsvgFormat::Json;
        } else if (arg == "-") {
            std::string filter;
            while (std::cin >> filter) filters.push_back(filter);
        } else if (!arg.empty() && arg[0] != '-') {
            targetVG = arg;
        }
    }
    return true;
}

void CmdOptions::PrintHelp() const {
    std::cout << R"(lsvg(1)                  CrossShell for UNIX Reference Manual                 lsvg(1)

NAME
    lsvg - report Windows volume groups

SYNOPSIS
    lsvg [OPTIONS] [VOLUME_GROUP]
    lsvg -l VOLUME_GROUP
    lsvg -p VOLUME_GROUP
    lsvg -a

DESCRIPTION
    Reports Windows storage architectures. Physical
    drives and logical disks are represented as volume groups, physical
    partitions, and logical volumes.

OPTIONS
    -o
        Select the active-volume-group mode.

    -l VOLUME_GROUP
        Display logical-volume details for a volume group.

    -p VOLUME_GROUP
        Display physical-volume details for a volume group.

    -a
        Display detailed status for all volume groups.

    --table
        Use aligned table output (default).

    --csv
        Emit volume-group summaries as CSV.

    --json
        Emit volume-group summaries as JSON.

    -
        Read volume-group filters from standard input.

    -h, --help, /?
        Display this comprehensive reference manual and exit.

    -V, --version
        Display version information and exit.

EXAMPLES
    lsvg
        Display the volume-group summary.

    lsvg -o
        Select active volume groups.

    lsvg rootvg
        Display the summary for rootvg.

    lsvg -l rootvg
        Display logical-volume details for rootvg.

    lsvg -p rootvg
        Display physical-volume details for rootvg.

    lsvg -a
        Display detailed summaries for all volume groups.

EXIT STATUS
    0
        Successful report generation.
    1
        Invalid options or unknown volume group.

CrossShell for UNIX                                                    lsvg(1)
)";
}

void CmdOptions::PrintVersion() const {
    std::cout << "lsvg 1.0.0\n";
}
