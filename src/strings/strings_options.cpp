#include "strings_options.hpp"

void StringsOptions::printUsage(const char* progName) {
        std::cout << R"(strings(1)              CrossShell for UNIX Reference Manual                 strings(1)

    NAME
        strings - print the sequences of printable characters in files

    SYNOPSIS
        strings [OPTIONS] [FILE]...

    DESCRIPTION
        For each FILE, prints the sequences of printable characters that are at
        least 4 characters long (or the number specified by -n). Reads standard
        input when FILE is '-' or omitted.

    OPTIONS
        -n, --bytes N
            Locate and print sequences of at least N printable characters (default: 4).

        --json
            Emit string matches in JSON format.

        --csv
            Emit string matches in CSV format.

        --table
            Emit string matches in tabular format.

        --pipe COMMAND
            Stream results directly into COMMAND.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        strings executable.exe
            Print printable character sequences in executable.exe.

        strings -n 8 /bin/ls
            Find strings at least 8 characters long.

        strings --json libtest.dll
            Extract strings from DLL formatted as JSON records.

    CrossShell for UNIX                                                    strings(1)
    )";
    }

void StringsOptions::printVersion() {
        std::cout << "strings v1.0.0\n";
    }

bool StringsOptions::parseUnsigned(const std::string& text, size_t& outValue) {
        if (text.empty()) return false;
        char* end = nullptr;
        unsigned long long value = std::strtoull(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0' || value == 0) return false;
        outValue = static_cast<size_t>(value);
        return true;
    }

bool StringsOptions::parse(int argc, char* argv[], StringsOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            }
            if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            }
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe") {
                if (i + 1 >= argc) return false;
                opts.pipeCommand = argv[++i];
                continue;
            }
            if (arg == "-n" || arg == "--bytes") {
                if (i + 1 >= argc) {
                    std::cerr << "strings: missing value for " << arg << "\n";
                    return false;
                }
                if (!parseUnsigned(argv[++i], opts.minLength)) {
                    std::cerr << "strings: invalid minimum length: " << argv[i] << "\n";
                    return false;
                }
                continue;
            }
            if (!arg.empty() && arg[0] == '-') {
                if (parseUnsigned(arg.substr(1), opts.minLength)) {
                    continue;
                }
                std::cerr << "strings: unrecognized option: " << arg << "\n";
                printUsage(argv[0]);
                return false;
            }
            opts.files.push_back(arg);
        }

        if (opts.files.empty()) {
            opts.files.push_back("-");
        }

        return true;
    }
