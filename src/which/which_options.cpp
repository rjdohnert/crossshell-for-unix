#include "which_options.hpp"

void WhichOptions::printUsage(const char* progName) {
        std::cout << "Usage: " << progName << " [OPTION]... [--] COMMAND ...\n"
                  << "Show the full path of commands that would be executed.\n\n"
                  << "Options:\n"
                  << "  -a, --all           print all matches in PATH, not just the first\n"
                  << "  -s, --skip-tilde    suppress output and return status only\n"
                  << "      --json, --csv   output structured matches\n"
                  << "      --pipe COMMAND  send output through COMMAND\n"
                  << "  -h, --help          display this help and exit\n"
                  << "      --version       output version information and exit\n";
    }

void WhichOptions::printVersion() {
        std::cout << "which 1.0\n";
    }

bool WhichOptions::parse(int argc, char* argv[], WhichOptions& opts) {
        bool parseOptions = true;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (parseOptions && arg == "--") {
                parseOptions = false;
                continue;
            }

            if (parseOptions && (arg == "-h" || arg == "--help" || arg == "/?")) {
                printUsage(argv[0]);
                std::exit(0);
            } else if (parseOptions && arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (parseOptions && (arg == "-a" || arg == "--all")) {
                opts.all = true;
            } else if (parseOptions && (arg == "-s" || arg == "--skip-tilde")) {
                opts.silent = true;
            } else if (parseOptions && arg == "--json") {
                opts.outputFormat = 1;
            } else if (parseOptions && arg == "--csv") {
                opts.outputFormat = 2;
            } else if (parseOptions && arg == "--table") {
                opts.outputFormat = 3;
            } else if (parseOptions && arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (parseOptions && arg[0] == '-' && arg.size() > 1) {
                std::cerr << "which: unknown option: " << arg << "\n";
                return false;
            } else {
                opts.commands.push_back(arg);
            }
        }

        if (opts.commands.empty()) {
            printUsage(argv[0]);
            return false;
        }

        return true;
    }
