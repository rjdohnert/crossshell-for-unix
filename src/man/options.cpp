#include "options.hpp"

void PrintUsage(const char* exe) {
    (void)exe;
    std::cout << R"(man(1)                  CrossShell for UNIX Reference Manual                  man(1)

    NAME
        man - format and display the on-line manual pages

    SYNOPSIS
        man [OPTIONS] [SECTION] <PAGE_NAME>
        man -l, --local-file <FILE_PATH>
        man -k, --keyword <KEYWORD>
        man -w, --where <PAGE_NAME>

    DESCRIPTION
        Displays the on-line reference manual pages for commands, system calls,
        and utilities. Searches configured manual paths and renders troff/groff,
        markdown, PDF, or plain text documentation.

    OPTIONS
        -l, --local-file <file>
            Open a specific local file instead of searching the manual path.

        -k, --keyword <string>
            Search manual page names and descriptions for a keyword.

        -w, --where <page>
            Print the resolved path to the manual file without displaying it.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        man ls
            Display the manual page for ls.

        man 1 printf
            Display the section 1 manual page for printf.

        man -k network
            Search all manual pages mentioning the keyword network.

        man -l ./docs/custom.1
            Render and view a local roff manual file.

    EXIT STATUS
        0
            Success.

        1
            An error occurred or the manual page was not found.

    CrossShell for UNIX                                                       man(1)
)";
}

void PrintVersion() {
    std::cout << "man v1.0.0\n";
}

bool parse_options(int argc, char* argv[], ManOptions& opts) {
    std::vector<std::string> positionalArgs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                positionalArgs.push_back(argv[j]);
            }
            break;
        } else if (arg == "-h" || arg == "--help" || arg == "/?") {
            opts.showHelp = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            opts.showVersion = true;
            return true;
        } else if (arg == "-l" || arg == "--local-file") {
            if (i + 1 >= argc) {
                std::cerr << Style::FG_RED << "man: option requires an argument -- '" << arg << "'" << Style::RESET << "\n";
                return false;
            }
            opts.forceLocal = true;
            opts.query = argv[++i];
        } else if (arg == "-k" || arg == "--keyword") {
            if (i + 1 >= argc) {
                std::cerr << Style::FG_RED << "man: option requires an argument -- '" << arg << "'" << Style::RESET << "\n";
                return false;
            }
            opts.keywordSearch = true;
            opts.keyword = argv[++i];
            return true;
        } else if (arg == "-w" || arg == "--where") {
            if (i + 1 >= argc) {
                std::cerr << Style::FG_RED << "man: option requires an argument -- '" << arg << "'" << Style::RESET << "\n";
                return false;
            }
            opts.showWhere = true;
            opts.query = argv[++i];
        } else if (opts.query.empty() && std::all_of(arg.begin(), arg.end(), ::isdigit)) {
            opts.section = arg;
        } else if (opts.query.empty()) {
            opts.query = arg;
        } else {
            positionalArgs.push_back(arg);
        }
    }

    if (!positionalArgs.empty() && opts.query.empty()) {
        opts.query = positionalArgs.front();
    }

    return true;
}
