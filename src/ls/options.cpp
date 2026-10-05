#include "options.hpp"
#include <iostream>
#include <cstdlib>

void HelpFormatter::printVersion() {
    std::cout << "ls 1.0.0\n";
}

void HelpFormatter::printHelp() {
    std::cout << R"(ls(1)               CrossShell for UNIX Reference Manual                 ls(1)

    NAME
        ls - list directory contents with native Windows attributes

    SYNOPSIS
        ls [OPTIONS] [FILE...]

    DESCRIPTION
        The ls command lists information about files and directories. For
        compatibility on Windows NT environments, it displays native Windows
        filesystem attributes without artificial POSIX emulation.

    WINDOWS FILE ATTRIBUTES
        Under long listing format (-l), file modes are formatted as an authentic
        9-character Windows mask:
            Position 1: 'd' = Directory
            Position 2: 'r' = Read-Only
            Position 3: 'a' = Archive
            Position 4: 'h' = Hidden
            Position 5: 's' = System
            Position 6: 'c' = Compressed
            Position 7: 'e' = Encrypted
            Position 8: 'l' = Reparse Point / Symbolic Link
            Position 9: '+' = Discretionary ACL Present

    OPTIONS
        -a, --all
            List all entries including hidden files, system files, and . / ..

        -A, --almost-all
            List all entries including hidden files, omitting . and ..

        -F, --classify
            Append indicator: '/' for directories, '*' for executables, '@' for links.

        -l, --long
            Use long listing format displaying attributes, owner, size, and date.

        -R, --recursive
            Recursively list directory subtrees.

        -r, --reverse
            Reverse the sorting order.

        -t
            Sort entries by modification timestamp (most recent first).

        -S
            Sort entries by file size (largest first).

        -1
            Force single-column output format.

        --no-color
            Disable ANSI color sequences in output.

        --output FORMAT
            Select table, csv, or json output format.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        ls -laF
            List all files with details and type indicators.

        ls -ltr C:\Projects
            List directory sorted by modification time in ascending order.

        ls --json | jq '.[].name'
            Output file list as structured JSON for jq pipeline processing.

    CrossShell for UNIX                                                    ls(1)
)";
}

bool ArgumentParser::parse(int argc, char* argv[], ListingOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            HelpFormatter::printHelp();
            exit(0);
        }
        if (arg == "-V" || arg == "--version") {
            HelpFormatter::printVersion();
            exit(0);
        }
        if (arg == "--no-color") {
            options.color = false;
            continue;
        }
        if (arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "ls: missing argument for --output\n";
                return false;
            }
            options.outputFormat = argv[++i];
            if (options.outputFormat != "json" && options.outputFormat != "csv" && options.outputFormat != "table") {
                std::cerr << "ls: invalid output format '" << options.outputFormat << "'\n";
                return false;
            }
            continue;
        }
        if (arg == "--json") {
            options.outputFormat = "json";
            continue;
        }
        if (arg == "--csv") {
            options.outputFormat = "csv";
            continue;
        }
        if (arg == "--table") {
            options.outputFormat = "table";
            continue;
        }

        if (arg.rfind("--", 0) == 0) {
            std::cerr << "ls: unrecognized option '" << arg << "'\nTry 'ls --help' for manual.\n";
            return false;
        }

        if (arg.length() > 1 && arg[0] == '-') {
            for (size_t c = 1; c < arg.length(); ++c) {
                switch (arg[c]) {
                    case 'a': options.all = true; break;
                    case 'A': options.almostAll = true; break;
                    case 'l': options.longFormat = true; break;
                    case 'F': options.typeIndicator = true; break;
                    case 'R': options.recursive = true; break;
                    case 'r': options.reverseSort = true; break;
                    case 't': options.sortByTime = true; break;
                    case 'S': options.sortBySize = true; break;
                    case '1': options.singleColumn = true; break;
                    default:
                        std::cerr << "ls: illegal option -- " << arg[c] << "\n";
                        std::cerr << "usage: ls [-a | -A] [-l] [-F] [-R] [-r] [-t] [-S] [-1] [file ...]\n";
                        return false;
                }
            }
        } else {
            options.targets.push_back(arg);
        }
    }

    if (options.targets.empty()) {
        options.targets.push_back(".");
    }

    return true;
}
