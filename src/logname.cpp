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
#include <lmcons.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. LOGIN NAME PROVIDER
// ============================================================================

class LoginNameProvider {
public:
    static std::string GetLoginName() {
        const char* envLogname = std::getenv("LOGNAME");
        if (envLogname != nullptr && envLogname[0] != '\0') {
            return std::string(envLogname);
        }

        const char* envUser = std::getenv("USER");
        if (envUser != nullptr && envUser[0] != '\0') {
            return std::string(envUser);
        }

        const char* envUsername = std::getenv("USERNAME");
        if (envUsername != nullptr && envUsername[0] != '\0') {
            return std::string(envUsername);
        }

        wchar_t username[UNLEN + 1] = { 0 };
        DWORD size = UNLEN + 1;
        if (GetUserNameW(username, &size)) {
            int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, username, -1, nullptr, 0, nullptr, nullptr);
            if (sizeNeeded > 1) {
                std::string str(static_cast<size_t>(sizeNeeded - 1), '\0');
                WideCharToMultiByte(CP_UTF8, 0, username, -1, &str[0], sizeNeeded, nullptr, nullptr);
                return str;
            }
        }

        return "";
    }
};

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class LognameOptions {
public:
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
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

    void PrintUsage(const char* progName) const {
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

    void PrintVersion() const {
        std::cout << "logname 1.0.0\n";
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class LognameApplication {
public:
    int Run(int argc, char* argv[]) {
        LognameOptions opts;
        if (!opts.Parse(argc, argv)) {
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : "logname");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        std::string logname = LoginNameProvider::GetLoginName();
        if (!logname.empty()) {
            std::cout << logname << "\n";
            return 0;
        }

        std::cerr << "logname: failed to get effective user name\n";
        return 1;
    }
};

int main(int argc, char* argv[]) {
    LognameApplication app;
    return app.Run(argc, argv);
}
