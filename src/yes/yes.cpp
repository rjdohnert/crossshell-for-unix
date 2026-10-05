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
 * SINGLE FILE INDEX: yes.cpp
 * ============================================================================
 * WinYes - Object-Oriented High-Throughput String Stream Repeater for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. YesOptions class (CLI parsing, help, version)
 * 2. [CORE REPEATER ENGINE] ................ YesEngine class (64KB buffered Win32 writer)
 * 3. [APPLICATION CONTROLLER] .............. YesApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class YesOptions {
public:
    static constexpr const char* VERSION = "1.0.0";
    std::string line{"y"};

    static void printHelp(const char* progName) {
        (void)progName;
        std::cout << R"(yes(1)                    CrossShell for UNIX Reference Manual                  yes(1)

    NAME
        yes - output a string repeatedly until killed

    SYNOPSIS
        yes [OPTIONS] [STRING...]

    DESCRIPTION
        Repeatedly output a line with all specified STRING(s), or 'y'.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        --
            Explicitly treat all subsequent arguments as input strings, even
            if they start with '-'.

        -h, --help, /?, -?
            Display this help and exit.

        -v, --version
            Output version information and exit.

    EXAMPLES
        yes
            Repeatedly output 'y'.

        yes confirmation
            Repeatedly output 'confirmation'.

        yes -- -h
            Repeatedly output '-h' without triggering help.

    CrossShell for UNIX                                                    yes(1)
)";
    }

    static void printVersion() {
        std::cout << "yes (CrossShell) " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

    static bool parse(int argc, char* argv[], YesOptions& opts) {
        if (argc > 1) {
            std::string firstArg = argv[1];
            if (firstArg == "-h" || firstArg == "--help" || firstArg == "/?" || firstArg == "-?") {
                printHelp(argv[0]);
                std::exit(0);
            }
            if (firstArg == "-v" || firstArg == "--version") {
                printVersion();
                std::exit(0);
            }

            int startIndex = 1;
            if (firstArg == "--") {
                startIndex = 2;
            }

            std::string lineStr;
            for (int i = startIndex; i < argc; ++i) {
                if (i > startIndex) lineStr += " ";
                lineStr += argv[i];
            }
            if (!lineStr.empty()) {
                opts.line = lineStr;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. CORE REPEATER ENGINE
// ============================================================================

class YesEngine {
private:
    static inline volatile LONG stopRequested = 0;
    static constexpr size_t BUFFER_SIZE = 64 * 1024;

    static BOOL WINAPI consoleControlHandler(DWORD controlType) {
        if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT) {
            InterlockedExchange(&stopRequested, 1);
            return TRUE;
        }
        return FALSE;
    }

public:
    static int execute(const std::string& inputLine) {
        std::string line = inputLine + "\n";

        std::vector<char> buffer;
        if (line.size() < BUFFER_SIZE) {
            buffer.reserve(BUFFER_SIZE);
            while (buffer.size() + line.size() <= BUFFER_SIZE) {
                buffer.insert(buffer.end(), line.begin(), line.end());
            }
        }

        const char* writePtr = buffer.empty() ? line.c_str() : buffer.data();
        DWORD writeSize = buffer.empty() ? static_cast<DWORD>(line.size()) : static_cast<DWORD>(buffer.size());

        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout == INVALID_HANDLE_VALUE || hStdout == NULL) {
            std::cerr << "yes: unable to access standard output handle\n";
            return 1;
        }

        if (!SetConsoleCtrlHandler(consoleControlHandler, TRUE)) {
            std::cerr << "yes: unable to install console control handler\n";
            return 1;
        }

        DWORD bytesWritten = 0;
        while (InterlockedCompareExchange(&stopRequested, 0, 0) == 0) {
            BOOL result = WriteFile(hStdout, writePtr, writeSize, &bytesWritten, NULL);

            if (InterlockedCompareExchange(&stopRequested, 0, 0) != 0) {
                break;
            }

            if (!result) {
                DWORD err = GetLastError();
                if (err == ERROR_NO_DATA || err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF) {
                    break;
                }
                break;
            }
        }

        SetConsoleCtrlHandler(consoleControlHandler, FALSE);
        return 0;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class YesApp {
public:
    static int run(int argc, char* argv[]) {
        YesOptions options;
        if (!YesOptions::parse(argc, argv, options)) {
            return 1;
        }
        return YesEngine::execute(options.line);
    }
};

int main(int argc, char* argv[]) {
    return YesApp::run(argc, argv);
}