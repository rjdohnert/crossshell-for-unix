/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "user32.lib")

// ============================================================================
// 1. SESSION CONTROLLER
// ============================================================================

class SessionManager {
public:
    static bool TerminateSession(bool force) {
        UINT flags = EWX_LOGOFF;
        if (force) {
            flags |= EWX_FORCE;
        }

        if (ExitWindowsEx(flags, 0)) {
            return true;
        }

        DWORD err = GetLastError();
        std::cerr << "logout: ExitWindowsEx failed with error code: " << err << std::endl;
        return false;
    }
};

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class LogoutOptions {
public:
    bool force = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
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

    void PrintUsage(const char* progName) const {
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

    void PrintVersion() const {
        std::cout << "logout 1.0.0\n";
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class LogoutApplication {
public:
    int Run(int argc, char* argv[]) {
        LogoutOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : "logout");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : "logout");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (!opts.force) {
            std::cout << "Are you sure you want to log out? (y/n): ";
            char response = 0;
            if (!(std::cin >> response) || (response != 'y' && response != 'Y')) {
                std::cout << "Logout canceled." << std::endl;
                return 0;
            }
        }

        std::cout << "Attempting to log out..." << std::endl;
        if (!SessionManager::TerminateSession(opts.force)) {
            std::cerr << "Could not initiate logout." << std::endl;
            return 1;
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    LogoutApplication app;
    return app.Run(argc, argv);
}
