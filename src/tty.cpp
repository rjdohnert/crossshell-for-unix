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
 * SINGLE FILE INDEX: tty.cpp
 * ============================================================================
 * WinTty - Object-Oriented Terminal Device Detector for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. TtyOptions class (CLI parsing, help)
 * 2. [TERMINAL DETECTOR ENGINE] ............ TtyEngine class (Console / PTY device inspector)
 * 3. [APPLICATION CONTROLLER] .............. TtyApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class TtyOptions {
public:
    bool silent{false};

    static void printHelp() {
        std::wcout << LR"(tty(1)                  CrossShell for UNIX Reference Manual                 tty(1)

    NAME
        tty - print the file name of the terminal connected to standard input

    SYNOPSIS
        tty [OPTIONS]

    DESCRIPTION
        Print the file name of the terminal connected to standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -s, -q, --silent, --quiet
            Print nothing, only return an exit status.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        tty
            Print name of current terminal device.

        tty -s
            Check if standard input is a terminal without producing output.

    CrossShell for UNIX                                                      tty(1)
)";
    }

    static void printVersion() {
        std::wcout << L"tty 1.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], TtyOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-s" || arg == L"-q" || arg == L"--silent" || arg == L"--quiet") {
                opts.silent = true;
            } else if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"--version" || arg == L"-v") {
                printVersion();
                std::exit(0);
            } else {
                std::wcerr << L"tty: invalid option: " << arg << L"\n";
                std::wcerr << L"Try 'tty --help' for more information.\n";
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. TERMINAL DETECTOR ENGINE
// ============================================================================

class TtyEngine {
public:
    static bool isTerminal(HANDLE hInput, std::wstring& ttyName) {
        if (hInput == INVALID_HANDLE_VALUE || hInput == NULL) {
            return false;
        }

        // 1. Check for Native Windows Console (cmd.exe, PowerShell, Windows Terminal)
        DWORD consoleMode = 0;
        if (GetConsoleMode(hInput, &consoleMode)) {
            ttyName = L"CONIN$";
            return true;
        }

        // 2. Check for MSYS2 / Cygwin / Git Bash PTY Pseudo-terminals
        DWORD fileType = GetFileType(hInput);
        if (fileType == FILE_TYPE_PIPE) {
            wchar_t pipeName[MAX_PATH] = { 0 };
            if (GetFinalPathNameByHandleW(hInput, pipeName, MAX_PATH, VOLUME_NAME_NT) > 0) {
                std::wstring name(pipeName);
                if (name.find(L"msys-") != std::wstring::npos ||
                    name.find(L"cygwin-") != std::wstring::npos ||
                    name.find(L"pty") != std::wstring::npos) {
                    ttyName = name;
                    return true;
                }
            }
        }

        return false;
    }

    static int execute(const TtyOptions& opts) {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        std::wstring ttyName;

        if (isTerminal(hStdin, ttyName)) {
            if (!opts.silent) {
                std::wcout << ttyName << L"\n";
            }
            return 0; // 0 = Connected to TTY
        } else {
            if (!opts.silent) {
                std::wcout << L"not a tty\n";
            }
            return 1; // 1 = Not a TTY
        }
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class TtyApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        TtyOptions options;
        if (!TtyOptions::parse(argc, argv, options)) {
            return 2;
        }
        return TtyEngine::execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return TtyApp::run(argc, argv);
}
