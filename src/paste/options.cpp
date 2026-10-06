#include "options.hpp"

void OptionParser::PrintUsage(const char* prog_name) {
    std::cout << R"(paste(1)                CrossShell for UNIX Reference Manual                  paste(1)

    NAME
        paste - merge lines of files

    SYNOPSIS
        paste [OPTIONS] [FILE...]

    DESCRIPTION
        paste writes lines consisting of the sequentially corresponding lines
        read from each FILE, separated by TABs, to standard output. With no
        FILE, or when FILE is '-', paste reads standard input.

    OPTIONS
        -d, --delimiters LIST
            Reuse characters from LIST instead of TABs. Escape sequences
            such as \n, \t, \r, \f, \0, and \\ are recognized.

        -s, --serial
            Paste one file at a time serially instead of in parallel.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        paste file1.txt file2.txt
            Merge lines from file1 and file2 side-by-side with tabs.

        paste -d "," file1.csv file2.csv
            Merge corresponding lines using comma delimiter.

        paste -s -d "\t\n" names.txt
            Format items in pairs on each output line.

        cat list.txt | paste -d " - " - descriptions.txt
            Combine standard input with lines from descriptions.txt.

    CrossShell for UNIX                                                    paste(1)
)";
}

void OptionParser::PrintVersion() {
    std::cout << "paste v1.0.0\n";
}

bool OptionParser::Parse(int argc, char* argv[], PasteOptions& opts) const {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                opts.files.push_back(argv[j]);
            }
            break;
        } else if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return true;
        } else if (arg == "--version" || arg == "-V") {
            opts.show_version = true;
            return true;
        } else if (arg == "-s" || arg == "--serial") {
            opts.serial = true;
        } else if (arg == "-d" || arg == "--delimiters") {
            if (i + 1 < argc) {
                opts.delim_list = argv[++i];
            } else {
                std::cerr << "paste: option requires an argument -- '" << arg << "'\n";
                return false;
            }
        } else if (arg.rfind("-d", 0) == 0 && arg.length() > 2) {
            opts.delim_list = arg.substr(2);
        } else if (arg.rfind("--delimiters=", 0) == 0) {
            opts.delim_list = arg.substr(13);
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
            // Compound short flags, e.g. -sd, -s
            bool valid = true;
            for (size_t c = 1; c < arg.length(); ++c) {
                if (arg[c] == 's') {
                    opts.serial = true;
                } else if (arg[c] == 'd') {
                    if (c + 1 < arg.length()) {
                        opts.delim_list = arg.substr(c + 1);
                        break;
                    } else if (i + 1 < argc) {
                        opts.delim_list = argv[++i];
                        break;
                    } else {
                        std::cerr << "paste: option requires an argument -- 'd'\n";
                        return false;
                    }
                } else {
                    valid = false;
                    std::cerr << "paste: invalid option -- '" << arg[c] << "'\n";
                    break;
                }
            }
            if (!valid) {
                return false;
            }
        } else {
            opts.files.push_back(arg);
        }
    }
    if (opts.files.empty()) {
        opts.files.push_back("-");
    }
    return true;
}
