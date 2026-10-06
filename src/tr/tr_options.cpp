#include "tr_options.hpp"

void TrOptions::printHelp() {
        std::cout << R"(tr(1)                   CrossShell for UNIX Reference Manual                 tr(1)

    NAME
        tr - translate or delete characters

    SYNOPSIS
        tr [OPTIONS] STRING1 [STRING2]

    DESCRIPTION
        Translate, squeeze, and/or delete characters from standard input,
        writing to standard output.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -c, -C, --complement
            Use the complement of STRING1.

        -d, --delete
            Delete characters in STRING1, do not translate.

        -s, --squeeze-repeats
            Replace each sequence of a repeated character that is listed in the
            last specified STRING with a single occurrence of that character.

        --json, --csv, --table
            Format transformed output records.

        --pipe COMMAND
            Send formatted output through COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        tr a-z A-Z
            Translate lowercase characters to uppercase.

        tr -d 0-9
            Delete all digits from input.

        tr -s " "
            Squeeze consecutive spaces into single spaces.

    CrossShell for UNIX                                                      tr(1)
)";
    }

bool TrOptions::parse(int argc, char* argv[], TrOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                printHelp();
                std::exit(0);
            }
            if (arg == "-v" || arg == "--version") {
                std::cout << "tr version 1.0.0\n";
                std::exit(0);
            } else if (arg == "-c" || arg == "-C" || arg == "--complement") {
                opts.complement = true;
            } else if (arg == "-d" || arg == "--delete") {
                opts.deleteChars = true;
            } else if (arg == "-s" || arg == "--squeeze-repeats") {
                opts.squeeze = true;
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char ch = arg[j];
                    if (ch == 'c' || ch == 'C') opts.complement = true;
                    else if (ch == 'd' || ch == 'D') opts.deleteChars = true;
                    else if (ch == 's') opts.squeeze = true;
                    else {
                        std::cerr << "tr: invalid option -- '" << ch << "'\n";
                        return false;
                    }
                }
            } else {
                opts.args.push_back(arg);
            }
        }

        if (opts.args.empty()) {
            std::cerr << "tr: missing operand\nTry 'tr --help' for more information.\n";
            return false;
        }

        return true;
    }
