#include "uniq_options.hpp"

void UniqOptions::showHelp() {
        std::cout << R"(uniq(1)                 CrossShell for UNIX Reference Manual                 uniq(1)

    NAME
        uniq - report or omit repeated lines

    SYNOPSIS
        uniq [OPTIONS] [INPUT [OUTPUT]]

    DESCRIPTION
        Filter adjacent matching lines from INPUT (or standard input), writing
        to OUTPUT (or standard output).
        With no options, matching lines are merged to the first occurrence.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -c, --count
            Prefix lines by the number of occurrences.

        -d, --repeated
            Only print duplicate lines, one for each group.

        -D
            Print all duplicate lines.

        --all-repeated[=METHOD]
            Like -D, but allow custom separation: none, prepend, separate.

        -f, --skip-fields=N
            Avoid comparing the first N whitespace-separated fields.

        -i, --ignore-case
            Ignore differences in case when comparing.

        -s, --skip-chars=N
            Avoid comparing the first N characters.

        -u, --unique
            Only print unique lines (occurring exactly once).

        -w, --check-chars=N
            Compare at most N characters in lines.

        -z, --zero-terminated
            Line delimiter is NUL (\0), not newline.

        --group[=METHOD]
            Separate groups using METHOD: separate, prepend, append, both.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        sequence log.txt | uniq -c
            Count duplicate occurrences in a sorted log.

        uniq -d input.txt duplicates.txt
            Write only duplicate lines to duplicates.txt.

        uniq -i -f 2 data.csv
            Case-insensitive deduplication, ignoring first 2 fields.

    CrossShell for UNIX                                                      uniq(1)
)";
    }

void UniqOptions::showVersion() {
        std::cout << "uniq 8.1\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }

UniqOptions UniqOptions::parse(int argc, char* argv[]) {
        UniqOptions opts;
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                showHelp();
                std::exit(0);
            } else if (arg == "--version" || arg == "-v" || arg == "-V") {
                showVersion();
                std::exit(0);
            } else if (arg == "-c" || arg == "--count") {
                opts.count = true;
            } else if (arg == "-d" || arg == "--repeated") {
                opts.repeatedOnly = true;
            } else if (arg == "-u" || arg == "--unique") {
                opts.uniqueOnly = true;
            } else if (arg == "-i" || arg == "--ignore-case") {
                opts.ignoreCase = true;
            } else if (arg == "-z" || arg == "--zero-terminated") {
                opts.zeroTerminated = true;
            } else if (arg == "-D") {
                opts.allRepeated = AllRepeatedMode::None;
                opts.repeatedOnly = true;
            } else if (arg.rfind("--all-repeated", 0) == 0) {
                opts.repeatedOnly = true;
                size_t eqPos = arg.find('=');
                if (eqPos == std::string_view::npos || arg.substr(eqPos + 1) == "none") {
                    opts.allRepeated = AllRepeatedMode::None;
                } else if (arg.substr(eqPos + 1) == "prepend") {
                    opts.allRepeated = AllRepeatedMode::Prepend;
                } else if (arg.substr(eqPos + 1) == "separate") {
                    opts.allRepeated = AllRepeatedMode::Separate;
                }
            } else if (arg.rfind("--group", 0) == 0) {
                size_t eqPos = arg.find('=');
                if (eqPos == std::string_view::npos || arg.substr(eqPos + 1) == "separate") {
                    opts.groupMode = GroupMode::Separate;
                } else if (arg.substr(eqPos + 1) == "prepend") {
                    opts.groupMode = GroupMode::Prepend;
                } else if (arg.substr(eqPos + 1) == "append") {
                    opts.groupMode = GroupMode::Append;
                } else if (arg.substr(eqPos + 1) == "both") {
                    opts.groupMode = GroupMode::Both;
                }
            } else if (arg.rfind("-f", 0) == 0 && arg.length() > 2) {
                opts.skipFields = std::stoul(std::string(arg.substr(2)));
            } else if (arg == "-f" || arg == "--skip-fields") {
                if (i + 1 < argc) opts.skipFields = std::stoul(argv[++i]);
            } else if (arg.rfind("-s", 0) == 0 && arg.length() > 2) {
                opts.skipChars = std::stoul(std::string(arg.substr(2)));
            } else if (arg == "-s" || arg == "--skip-chars") {
                if (i + 1 < argc) opts.skipChars = std::stoul(argv[++i]);
            } else if (arg.rfind("-w", 0) == 0 && arg.length() > 2) {
                opts.checkChars = std::stoul(std::string(arg.substr(2)));
            } else if (arg == "-w" || arg == "--check-chars") {
                if (i + 1 < argc) opts.checkChars = std::stoul(argv[++i]);
            } else if (arg.length() > 1 && arg[0] == '-' && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'c': opts.count = true; break;
                        case 'd': opts.repeatedOnly = true; break;
                        case 'u': opts.uniqueOnly = true; break;
                        case 'i': opts.ignoreCase = true; break;
                        case 'z': opts.zeroTerminated = true; break;
                        case 'D': opts.allRepeated = AllRepeatedMode::None; opts.repeatedOnly = true; break;
                        default:
                            std::cerr << "uniq: invalid option -- '" << arg[j] << "'\n";
                            std::cerr << "Try 'uniq --help' for more information.\n";
                            std::exit(1);
                    }
                }
            } else {
                positional.push_back(std::string(arg));
            }
        }

        if (!positional.empty()) opts.inputPath = positional[0];
        if (positional.size() > 1) opts.outputPath = positional[1];

        return opts;
    }
