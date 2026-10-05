/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "options.hpp"
#include <iostream>
#include <string>

bool LogoutOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";

        if (arg == "--help" || arg == "-h") {
            showHelp = true;
            return true;
        }

        if (arg == "--version" || arg == "-V") {
            showVersion = true;
            return true;
        }

        if (arg == "--force" || arg == "-f") {
            force = true;
            continue;
        }

        if (arg == "--") {
            continue;
        }

        if (arg.rfind("--", 0) == 0 || (!arg.empty() && arg[0] == '-')) {
            std::cerr << "logout: unknown option -- " << arg << std::endl;
            return false;
        }

        std::cerr << "logout: unexpected argument: " << arg << std::endl;
        return false;
    }
    return true;
}

void LogoutOptions::PrintUsage(const char* /*progName*/) const {
    std::cout << R"(logout(1)               CrossShell for UNIX Reference Manual                logout(1)

    NAME
        logout - log out of current Windows desktop or terminal session

    SYNOPSIS
        logout [OPTIONS]

    DESCRIPTION
        logout terminates the current Windows user session, closing running
        applications or invoking ExitWindowsEx / WTSLogoffSession.

    OPTIONS
        -f, --force
            Force session termination without waiting for applications.

        --json, --csv, --table
            Output structured execution status.

        --pipe COMMAND
            Stream output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        logout --force
            Force immediate session logoff.

    CrossShell for UNIX                                                 logout(1)
)";
}

void LogoutOptions::PrintVersion() const {
    std::cout << "logout 1.0.0\n";
}
