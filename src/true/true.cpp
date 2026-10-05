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
 * SINGLE FILE INDEX: true.cpp
 * ============================================================================
 * WinTrue - Object-Oriented Success Status Return Utility for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. TrueOptions class (CLI flags, help, version)
 * 2. [OUTPUT & PIPE DISPATCHER] ............ TrueReporter class (JSON/CSV/Table/Pipe)
 * 3. [APPLICATION CONTROLLER] .............. TrueApp class and wmain entry point
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

class TrueOptions {
public:
    int outputFormat{0}; // 0: none, 1: JSON, 2: CSV, 3: Table
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(true(1)                 CrossShell for UNIX Reference Manual                 true(1)

    NAME
        true - return successful exit status

    SYNOPSIS
        true [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Exit with a status code indicating success.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        --json
            Output success status as JSON format.

        --csv
            Output success status as CSV format.

        --table
            Output success status as formatted table.

        --pipe COMMAND
            Send status through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        true
            Execute and return exit status code 0.

    CrossShell for UNIX                                                      true(1)
)";
    }

    static void printVersion() {
        std::wcout << L"true 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], TrueOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--help" || arg == L"-h" || arg == L"/?" || arg == L"-?") {
                printHelp();
                std::exit(0);
            }
            if (arg == L"--version" || arg == L"-V" || arg == L"-v") {
                printVersion();
                std::exit(0);
            }
            if (arg == L"--json") opts.outputFormat = 1;
            else if (arg == L"--csv") opts.outputFormat = 2;
            else if (arg == L"--table") opts.outputFormat = 3;
            else if (arg == L"--pipe") {
                if (i + 1 >= argc) return false;
                opts.pipeCommand = argv[++i];
            }
        }
        return true;
    }
};

// ============================================================================
// 2. OUTPUT & PIPE DISPATCHER
// ============================================================================

class TrueReporter {
public:
    static std::string wideToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static int report(const TrueOptions& opts) {
        if (opts.outputFormat != 0 || !opts.pipeCommand.empty()) {
            std::wstring text = (opts.outputFormat == 1) ? L"{\"status\":\"success\",\"exit_code\":0}\n"
                              : (opts.outputFormat == 2) ? L"status,exit_code\nsuccess,0\n"
                              : L"STATUS\tEXIT_CODE\nsuccess\t0\n";

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
        return 0;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class TrueApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        TrueOptions options;
        if (!TrueOptions::parse(argc, argv, options)) {
            return 1;
        }
        return TrueReporter::report(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return TrueApp::run(argc, argv);
}
