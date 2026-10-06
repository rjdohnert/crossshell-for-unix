#include "counters.hpp"
#include "option_parser.hpp"
#include "wc_options.hpp"

void OptionParser::PrintUsage() {
        std::cout << R"(wc(1)                   CrossShell for UNIX Reference Manual                 wc(1)

    NAME
        wc - print newline, word, and byte counts for each file

    SYNOPSIS
        wc [OPTIONS] [FILE...]

    DESCRIPTION
        Print newline, word, and byte counts for each FILE, and a total line if
        more than one FILE is specified. A word is a non-zero-length sequence of
        printable characters delimited by white space.
        With no FILE, or when FILE is -, read standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -c, --bytes
            Print the byte counts.

        -m, --chars
            Print the character counts.

        -l, --lines
            Print the newline counts.

        -w, --words
            Print the word counts.

        -L, --max-line-length
            Print the maximum display width.

        --json, --csv, --table
            Select output format (JSON, CSV, or Table).

        --pipe COMMAND
            Send output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        wc file.txt
            Print line, word, and byte counts for file.txt.

        wc -l file1.txt file2.txt
            Print line counts for both files and total.

        dir | wc -l
            Count lines in directory listing.

    CrossShell for UNIX                                                      wc(1)
)";
    }

void OptionParser::PrintVersion() {
        std::cout << "wc 1.0.0\n";
    }

bool OptionParser::Parse(int argc, char* argv[], WcOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.files.push_back(argv[j]);
                }
                break;
            } else if (arg == "-") {
                opts.files.push_back("-");
            } else if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                PrintUsage();
                exitEarly = true;
                return true;
            } else if (arg == "-v" || arg == "-V" || arg == "--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg == "--bytes") {
                opts.opt_bytes = true;
            } else if (arg == "--chars") {
                opts.opt_chars = true;
            } else if (arg == "--lines") {
                opts.opt_lines = true;
            } else if (arg == "--words") {
                opts.opt_words = true;
            } else if (arg == "--max-line-length") {
                opts.opt_max_len = true;
            } else if (arg == "--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == "--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == "--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipe_command = argv[++i];
            } else if (arg.rfind("--", 0) == 0) {
                std::cerr << "wc: unknown option -- " << arg.substr(2) << "\n";
                PrintUsage();
                return false;
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::string opt_str = arg.substr(1);
                for (size_t j = 0; j < opt_str.size(); ++j) {
                    char opt = opt_str[j];
                    if (opt == 'c') opts.opt_bytes = true;
                    else if (opt == 'm') opts.opt_chars = true;
                    else if (opt == 'l') opts.opt_lines = true;
                    else if (opt == 'w') opts.opt_words = true;
                    else if (opt == 'L') opts.opt_max_len = true;
                    else if (opt == 'h' || opt == '?') {
                        PrintUsage();
                        exitEarly = true;
                        return true;
                    } else if (opt == 'V' || opt == 'v') {
                        PrintVersion();
                        exitEarly = true;
                        return true;
                    } else {
                        std::cerr << "wc: unknown option -- " << opt << "\n";
                        PrintUsage();
                        return false;
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        bool any_flag = (opts.opt_bytes || opts.opt_chars || opts.opt_lines || opts.opt_words || opts.opt_max_len);
        if (!any_flag) {
            opts.opt_lines = true;
            opts.opt_words = true;
            opts.opt_bytes = true;
        }

        if (opts.files.empty()) {
            opts.implicit_stdin = true;
            opts.files.push_back("-");
        }

        return true;
    }
