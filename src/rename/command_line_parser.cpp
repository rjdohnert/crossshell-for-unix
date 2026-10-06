#include "command_line_parser.hpp"
#include "rename_options.hpp"
#include "wildcard_expander.hpp"

bool CommandLineParser::parse(int argc, char* argv[], RenameOptions& options) {
        std::vector<std::string> positionalArgs;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                options.showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                options.showVersion = true;
                return true;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "-n" || arg == "--no-act" || arg == "--dry-run") {
                options.dryRun = true;
            } else if (arg == "-i" || arg == "--interactive") {
                options.interactive = true;
            } else if (arg == "-o" || arg == "--no-overwrite") {
                options.noOverwrite = true;
            } else if (arg == "-a" || arg == "--all") {
                options.replaceAll = true;
            } else if (arg == "-l" || arg == "--last") {
                options.replaceLast = true;
            } else if (arg == "-c" || arg == "--ignore-case") {
                options.ignoreCase = true;
            } else if (arg == "-e" || arg == "--regex") {
                options.useRegex = true;
            } else if (arg.rfind("--", 0) == 0) {
                std::cerr << "rename: unrecognized option '" << arg << "'\n";
                return false;
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1) {
                // Handle bundled short options (e.g. -va)
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'v': options.verbose = true; break;
                        case 'n': options.dryRun = true; break;
                        case 'i': options.interactive = true; break;
                        case 'o': options.noOverwrite = true; break;
                        case 'a': options.replaceAll = true; break;
                        case 'l': options.replaceLast = true; break;
                        case 'c': options.ignoreCase = true; break;
                        case 'e': options.useRegex = true; break;
                        default:
                            std::cerr << "rename: invalid option -- '" << arg[c] << "'\n";
                            return false;
                    }
                }
            } else {
                positionalArgs.push_back(arg);
            }
        }

        if (positionalArgs.size() < 3) {
            std::cerr << "rename: missing operand\n";
            return false;
        }

        options.searchPattern = positionalArgs[0];
        options.replacement   = positionalArgs[1];

        std::vector<std::string> rawFiles(positionalArgs.begin() + 2, positionalArgs.end());
        options.targetFiles = WildcardExpander::expand(rawFiles);

        return true;
    }
