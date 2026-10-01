/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: false.cpp
 * ============================================================================
 * WinFalse - Object-Oriented Failure Status Return Utility for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. FalseOptions class (CLI flags, help, version)
 * 2. [OUTPUT & PIPE DISPATCHER] ............ FalseReporter class (JSON/CSV/Table/Pipe)
 * 3. [APPLICATION CONTROLLER] .............. FalseApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class FalseOptions {
public:
    int outputFormat{0}; // 0: none, 1: JSON, 2: CSV, 3: Table
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(false(1)            CrossShell for UNIX Reference Manual                 false(1)

    NAME
        false - do nothing, unsuccessfully

    SYNOPSIS
        false [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Exit with a status code indicating failure (1). Ignores any command-line
        arguments provided, returning an exit code of 1 immediately.

    OPTIONS
        --json
            Output failure status formatted as JSON.

        --csv
            Output failure status formatted as CSV.

        --table
            Output failure status formatted as a table.

        --pipe COMMAND
            Send status output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        false
            Execute false and return exit code 1.

        false || echo "Command failed"
            Conditional execution upon failure.

    CrossShell for UNIX                                                    false(1)
)";
    }

    static void printVersion() {
        std::wcout << L"false 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], FalseOptions& opts) {
        if (argc > 1) {
            std::wstring arg = argv[1] ? argv[1] : L"";
            if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                printHelp();
                std::exit(0);
            }
            if (arg == L"--version" || arg == L"-V") {
                printVersion();
                std::exit(0);
            }
        }

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") opts.outputFormat = 1;
            else if (arg == L"--csv") opts.outputFormat = 2;
            else if (arg == L"--table") opts.outputFormat = 3;
            else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            }
        }
        return true;
    }
};

// ============================================================================
// 2. OUTPUT & PIPE DISPATCHER
// ============================================================================

class FalseReporter {
public:
    static std::string wideToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static int report(const FalseOptions& opts) {
        if (opts.outputFormat != 0 || !opts.pipeCommand.empty()) {
            std::wstring text = (opts.outputFormat == 1) ? L"{\"status\":\"failure\",\"exit_code\":1}\n"
                              : (opts.outputFormat == 2) ? L"status,exit_code\nfailure,1\n"
                              : L"STATUS\tEXIT_CODE\nfailure\t1\n";

            if (!opts.pipeCommand.empty()) {
                FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
                if (!pipe) return 1;
                std::string narrow = wideToUtf8(text);
                std::fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            } else {
                std::wcout << text;
            }
        }
        return 1;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class FalseApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        FalseOptions options;
        if (!FalseOptions::parse(argc, argv, options)) {
            return 1;
        }
        return FalseReporter::report(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return FalseApp::run(argc, argv);
}
