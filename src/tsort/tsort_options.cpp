#include "tsort_options.hpp"

void TsortOptions::printUsage() {
        std::cout << R"(tsort(1)                CrossShell for UNIX Reference Manual                 tsort(1)

    NAME
        tsort - perform topological sort

    SYNOPSIS
        tsort [OPTIONS] [FILE]

    DESCRIPTION
        Write totally ordered list consistent with the partial ordering in
        FILE. With no FILE, or when FILE is -, read standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -m, --multiple
            Process multiple input files in order.

        --json, --csv
            Output ordered nodes as structured JSON or CSV records.

        --table
            Output ordered nodes as a formatted table.

        --pipe COMMAND
            Send formatted output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        tsort pairs.txt
            Topologically sort dependency pairs from pairs.txt.

    CrossShell for UNIX                                                      tsort(1)
)";
    }

void TsortOptions::printVersion() {
        std::cout << "tsort 1.0\n";
    }

bool TsortOptions::parse(int argc, char* argv[], TsortOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                printUsage();
                std::exit(0);
            } else if (arg == "--version" || arg == "-v") {
                printVersion();
                std::exit(0);
            } else if (arg == "-m" || arg == "--multiple") {
                opts.multiple = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg != "-") {
                std::cerr << "tsort: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.filenames.push_back(arg);
            }
        }

        if (!opts.multiple && opts.filenames.size() > 1) {
            std::cerr << "tsort: too many arguments\n";
            return false;
        }

        if (opts.filenames.empty()) {
            opts.filenames.push_back("-");
        }

        return true;
    }
