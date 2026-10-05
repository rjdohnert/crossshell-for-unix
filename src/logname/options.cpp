/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "options.hpp"
#include <iostream>

bool LognameOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            showHelp = true;
            return true;
        }

        if (arg == "-V" || arg == "-v" || arg == "--version") {
            showVersion = true;
            return true;
        }

        if (arg == "--") {
            for (++i; i < argc; ++i) {
                std::cerr << "logname: extra operand '" << argv[i] << "'\n"
                          << "Try 'logname --help' for more information.\n";
                return false;
            }
            break;
        }

        if (arg.rfind("--", 0) == 0 || (!arg.empty() && arg[0] == '-')) {
            std::cerr << "logname: unknown option -- '" << arg << "'\n"
                      << "Try 'logname --help' for more information.\n";
            return false;
        }

        std::cerr << "logname: extra operand '" << arg << "'\n"
                  << "Try 'logname --help' for more information.\n";
        return false;
    }
    return true;
}

void LognameOptions::PrintUsage(const char* /*progName*/) const {
    std::cout << R"(logname(1)              CrossShell for UNIX Reference Manual               logname(1)

    NAME
        logname - print user's login name

    SYNOPSIS
        logname [OPTIONS]

    DESCRIPTION
        logname displays the login name of the current user session from the
        Windows environment and security context.

    OPTIONS
        --json, --csv, --table
            Format username record as JSON, CSV, or table.

        --pipe COMMAND
            Stream output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        logname
            Print current user name.

    CrossShell for UNIX                                                logname(1)
)";
}

void LognameOptions::PrintVersion() const {
    std::cout << "logname 1.0.0\n";
}
