#include "options.hpp"

Config CommandLineParser::Parse(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--human")        cfg.unit = DisplayUnit::Human;
        else if (arg == "-b" || arg == "--bytes")   cfg.unit = DisplayUnit::Bytes;
        else if (arg == "-k" || arg == "--kibi")    cfg.unit = DisplayUnit::Kibi;
        else if (arg == "-m" || arg == "--mebi")    cfg.unit = DisplayUnit::Mebi;
        else if (arg == "-g" || arg == "--gibi")    cfg.unit = DisplayUnit::Gibi;
        else if (arg == "--json")                   cfg.format = OutputFormat::Json;
        else if (arg == "--csv")                    cfg.format = OutputFormat::Csv;
        else if (arg == "--table")                  cfg.format = OutputFormat::Table;
        else if (arg == "-d" || arg == "--devices") cfg.showPagefileDetails = true;
        else if (arg == "-w" || arg == "--wide")    { cfg.showKernelDetails = true; cfg.showPagefileDetails = true; }
        else if (arg == "-s" || arg == "--seconds") {
            if (i + 1 < argc) cfg.repeatSeconds = std::stoi(argv[++i]);
        }
        else if (arg == "-c" || arg == "--count") {
            if (i + 1 < argc) cfg.maxCount = std::stoi(argv[++i]);
        }
        else if (arg == "/?" || arg == "--help")    cfg.showHelp = true;
        else if (arg == "-v" || arg == "--version") cfg.showVersion = true;
        else {
            std::cerr << "Unknown option: " << arg << " (Run meminfo --help for usage)\n";
        }
    }
    return cfg;
}

void CommandLineParser::PrintHelp() {
    std::cout <<
R"(meminfo - Physical and Virtual Swap Memory Reporter

USAGE:
  meminfo [QUALIFIERS]

DISPLAY UNITS:
  -h, --human        Show scaling human-readable format (e.g., 16.24 GiB) [Default]
  -b, --bytes        Show outputs in raw bytes
  -k, --kibi         Show outputs in KiB (1024 bytes)
  -m, --mebi         Show outputs in MiB (1048576 bytes)
  -g, --gibi         Show outputs in GiB (1073741824 bytes)

OUTPUT FORMATS (Pipeline friendly):
  --table            Standard ASCII grid output [Default]
  --json             Serialize snapshot as JSON (raw byte counts)
  --csv              Emit comma-separated values

TOPOLOGY & DETAILS:
  -d, --devices      Show per-device pagefile breakdown (like HP-UX swapinfo)
  -w, --wide         Show extended kernel pools (Paged/Non-Paged) and devices

MONITORING & INTERVALS:
  -s, --seconds <N>  Repeat report every N seconds
  -c, --count <N>    Exit after N iterations (used alongside -s)

INFORMATIONAL:
  -?, --help         Print this comprehensive help screen
  -v, --version      Display application version
)";
}
