#include "cli_parser.hpp"
#include "sync_options.hpp"

bool CliParser::parse(int argc, char* argv[], CliOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                opts.show_help = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                opts.show_version = true;
                return true;
            } else if (arg == "-f" || arg == "--file-system") {
                opts.file_system = true;
            } else if (arg == "-d" || arg == "--data") {
                opts.data_only = true;
            } else if (arg == "-r" || arg == "--removable") {
                opts.removable_only = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (!arg.empty() && arg[0] == '-') {
                // Short flags cluster (e.g. -rv)
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    if (c == 'f') opts.file_system = true;
                    else if (c == 'd') opts.data_only = true;
                    else if (c == 'r') opts.removable_only = true;
                    else if (c == 'v') opts.verbose = true;
                    else if (c == 'q') opts.quiet = true;
                    else if (c == 'h') { opts.show_help = true; return true; }
                    else if (c == 'V') { opts.show_version = true; return true; }
                    else {
                        std::cerr << "sync: unknown option -- '" << c << "'\n";
                        std::cerr << "Try 'sync --help' for more information.\n";
                        return false;
                    }
                }
            } else {
                opts.targets.push_back(arg);
            }
        }
        return true;
    }
