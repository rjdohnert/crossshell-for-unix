#include "options.hpp"
#include <iostream>
#include <cstdlib>

CommandLineOptions ArgumentParser::Parse(int argc, char* argv[]) const {
    CommandLineOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            opts.showHelp = true;
        } else if (arg == "-C") {
            opts.customizedMode = true;
            opts.predefinedMode = false;
        } else if (arg == "-P") {
            opts.predefinedMode = true;
            opts.customizedMode = false;
        } else if (arg == "-H") {
            opts.showHeaders = true;
        } else if (arg == "-c" && i + 1 < argc) {
            opts.filterClass = argv[++i];
        } else if (arg == "-s" && i + 1 < argc) {
            opts.filterSubclass = argv[++i];
        } else if (arg == "-t" && i + 1 < argc) {
            opts.filterType = argv[++i];
        } else if (arg == "-l" && i + 1 < argc) {
            opts.filterName = argv[++i];
        } else if (arg == "-S" && i + 1 < argc) {
            opts.filterState = argv[++i];
        } else if (arg == "-F" && i + 1 < argc) {
            opts.customFormat = argv[++i];
        } else if (arg == "-r" && i + 1 < argc) {
            opts.listColumn = argv[++i];
        } else {
            std::cerr << "lsdev: 0514-512 Invalid flag or missing parameter: " << arg << "\n";
            std::cerr << "Try 'lsdev --help' for more information.\n";
            std::exit(1);
        }
    }
    return opts;
}
