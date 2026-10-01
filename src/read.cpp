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
 * SINGLE FILE INDEX: read.cpp
 * ============================================================================
 * WinRead - Object-Oriented Standard Input & Console Line Reader for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. ReadOptions class (CLI parsing & options)
 * 2. [CONSOLE GUARD & OUTPUT DISPATCH] ..... ConsoleModeGuard and ReadReporter classes
 * 3. [INPUT READER ENGINES] ................ ConsoleReader and PipeReader classes
 * 4. [CORE READ ENGINE] .................... ReadEngine class (orchestration & execution)
 * 5. [APPLICATION CONTROLLER] .............. ReadApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cwchar>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

enum class ReadOutputFormat { Human, Json, Csv, Table };

class ReadOptions {
public:
    std::wstring prompt{L""};
    bool silent{false};
    bool raw{false};
    int maxChars{-1};         // -1 = unlimited
    double timeoutSec{-1.0};   // -1.0 = no timeout
    wchar_t delim{L'\n'};
    ReadOutputFormat outputFormat{ReadOutputFormat::Human};
    std::wstring pipeCommand;

    static void printUsage() {
        std::wcout << LR"(read(1)                     CrossShell for UNIX Reference Manual                    read(1)

    NAME
        read - read a line from standard input

    SYNOPSIS
        read [OPTIONS]

    DESCRIPTION
        read reads a single line from standard input (or console) into memory
        and outputs the result. It supports prompts, silent input (password
        masking), character limits, timeouts, custom delimiters, and structured
        output formatting.

    OPTIONS
        -p, --prompt <prompt>
            Display <prompt> string on standard error before reading.

        -s, --silent
            Do not echo typed characters (silent/password mode).

        -r, --raw
            Raw mode; do not allow backslash to escape characters.

        -n <nchars>
            Return immediately after reading <nchars> characters.

        -t <timeout>
            Time out after <timeout> seconds (supports decimal values, e.g. 2.5).

        -d <delim>
            Read until the first character of <delim> instead of newline.

        --json
            Output the result as JSON.

        --csv
            Output the result as CSV.

        --table
            Output the result as an ASCII table.

        --pipe <command>
            Pipe structured output through <command>.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        read -p "Enter username: "
            Prompt user and read input line.

        read -s -p "Enter password: "
            Read password securely without echoing characters.

        read -n 1 -p "Continue? (y/n) "
            Read exactly one character.

        read -t 5.0 -p "Press Enter within 5 seconds: "
            Read with a 5-second timeout.

    CrossShell for UNIX                                                          read(1)
)";
    }

    static void printVersion() {
        std::wcout << L"read 1.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], ReadOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage();
                std::exit(0);
            } else if (arg == L"--version" || arg == L"-V") {
                printVersion();
                std::exit(0);
            } else if ((arg == L"-p" || arg == L"--prompt") && i + 1 < argc) {
                opts.prompt = argv[++i];
            } else if (arg == L"-s" || arg == L"--silent") {
                opts.silent = true;
            } else if (arg == L"-r" || arg == L"--raw") {
                opts.raw = true;
            } else if (arg == L"-n" && i + 1 < argc) {
                opts.maxChars = std::wcstol(argv[++i], nullptr, 10);
            } else if (arg == L"-t" && i + 1 < argc) {
                opts.timeoutSec = std::wcstod(argv[++i], nullptr);
            } else if (arg == L"-d" && i + 1 < argc) {
                std::wstring d = argv[++i];
                if (!d.empty()) opts.delim = d[0];
            } else if (arg == L"--json") {
                opts.outputFormat = ReadOutputFormat::Json;
            } else if (arg == L"--csv") {
                opts.outputFormat = ReadOutputFormat::Csv;
            } else if (arg == L"--table") {
                opts.outputFormat = ReadOutputFormat::Table;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            }
        }
        return true;
    }
};

// ============================================================================
// 2. CONSOLE GUARD & OUTPUT DISPATCH
// ============================================================================

class ConsoleModeGuard {
private:
    HANDLE hIn;
    DWORD origMode{0};
    bool active{false};

public:
    explicit ConsoleModeGuard(HANDLE h) : hIn(h) {
        if (GetConsoleMode(hIn, &origMode)) {
            DWORD newMode = origMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
            if (SetConsoleMode(hIn, newMode)) {
                active = true;
            }
        }
    }

    ~ConsoleModeGuard() {
        restore();
    }

    void restore() {
        if (active) {
            SetConsoleMode(hIn, origMode);
            active = false;
        }
    }
};

class ReadReporter {
public:
    static std::wstring jsonQuote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            if (ch == L'"' || ch == L'\\') out += L'\\';
            if (ch == L'\n') out += L'n';
            else if (ch == L'\r') out += L'r';
            else if (ch == L'\t') out += L't';
            else out += ch;
        }
        return out + L"\"";
    }

    static std::wstring csvQuote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            out += (ch == L'"') ? L"\"\"" : std::wstring(1, ch);
        }
        return out + L"\"";
    }

    static void writeHandle(HANDLE hHandle, const std::wstring& text) {
        if (text.empty()) return;

        DWORD mode;
        if (GetConsoleMode(hHandle, &mode)) {
            DWORD written = 0;
            WriteConsoleW(hHandle, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
        } else {
            int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), NULL, 0, NULL, NULL);
            if (size > 0) {
                std::string utf8Str(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &utf8Str[0], size, NULL, NULL);
                DWORD written = 0;
                WriteFile(hHandle, utf8Str.data(), static_cast<DWORD>(utf8Str.length()), &written, NULL);
            }
        }
    }

    static int output(const std::wstring& result, const ReadOptions& opts, int exitCode) {
        std::wstring formatted;
        if (opts.outputFormat == ReadOutputFormat::Json) formatted = L"{\"value\":" + jsonQuote(result) + L"}\n";
        else if (opts.outputFormat == ReadOutputFormat::Csv) formatted = L"\"value\"\n" + csvQuote(result) + L"\n";
        else if (opts.outputFormat == ReadOutputFormat::Table) formatted = L"VALUE\n-----\n" + result + L"\n";
        else formatted = result;

        if (!opts.pipeCommand.empty()) {
            FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (!pipe) return (exitCode == 0) ? 1 : exitCode;
            int size = WideCharToMultiByte(CP_UTF8, 0, formatted.c_str(), static_cast<int>(formatted.length()), NULL, 0, NULL, NULL);
            if (size > 0) {
                std::string utf8(static_cast<size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, formatted.c_str(), static_cast<int>(formatted.length()), &utf8[0], size, NULL, NULL);
                std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            }
            std::fflush(pipe);
            _pclose(pipe);
        } else {
            writeHandle(GetStdHandle(STD_OUTPUT_HANDLE), formatted);
        }

        return exitCode;
    }
};

// ============================================================================
// 3. INPUT READER ENGINES
// ============================================================================

class ConsoleReader {
public:
    static int read(const ReadOptions& opts, std::wstring& result) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        ConsoleModeGuard guard(hIn);

        auto startTime = std::chrono::steady_clock::now();
        bool escapeNext = false;

        while (true) {
            if (opts.maxChars > 0 && static_cast<int>(result.length()) >= opts.maxChars) {
                break;
            }

            DWORD waitMs = INFINITE;
            if (opts.timeoutSec >= 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - startTime).count();
                double remaining = opts.timeoutSec - elapsed;
                if (remaining <= 0.0) {
                    return 142; // Timeout status code
                }
                waitMs = static_cast<DWORD>(remaining * 1000.0);
            }

            DWORD waitRes = WaitForSingleObject(hIn, waitMs);
            if (waitRes == WAIT_TIMEOUT) {
                return 142;
            }
            if (waitRes != WAIT_OBJECT_0) {
                return 1;
            }

            INPUT_RECORD ir;
            DWORD recordsRead = 0;
            if (!ReadConsoleInputW(hIn, &ir, 1, &recordsRead) || recordsRead == 0) {
                return 1;
            }

            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
                continue;
            }

            wchar_t ch = ir.Event.KeyEvent.uChar.UnicodeChar;
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;

            if (ch == 0) continue;

            if (ch == 3) return 130; // Ctrl+C
            if (ch == 26) break;     // Ctrl+Z (EOF)

            if (vk == VK_BACK || ch == L'\b') {
                if (!result.empty()) {
                    result.pop_back();
                    if (!opts.silent) {
                        DWORD written = 0;
                        WriteConsoleW(hOut, L"\b \b", 3, &written, NULL);
                    }
                }
                continue;
            }

            if (escapeNext && (ch == L'\r' || ch == L'\n')) {
                escapeNext = false;
                if (!opts.silent) {
                    DWORD written = 0;
                    WriteConsoleW(hOut, L"\r\n> ", 4, &written, NULL);
                }
                continue;
            }

            bool isDelim = false;
            if (opts.delim == L'\n' && (ch == L'\r' || ch == L'\n')) {
                isDelim = true;
            } else if (ch == opts.delim) {
                isDelim = true;
            }

            if (isDelim) {
                if (!opts.silent) {
                    DWORD written = 0;
                    WriteConsoleW(hOut, L"\r\n", 2, &written, NULL);
                }
                break;
            }

            if (!opts.raw && !escapeNext && ch == L'\\') {
                escapeNext = true;
                continue;
            }

            escapeNext = false;
            result.push_back(ch);

            if (!opts.silent) {
                DWORD written = 0;
                WriteConsoleW(hOut, &ch, 1, &written, NULL);
            }
        }

        return 0;
    }
};

class PipeReader {
public:
    static int read(const ReadOptions& opts, std::wstring& result) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        auto startTime = std::chrono::steady_clock::now();
        bool escapeNext = false;

        while (true) {
            if (opts.maxChars > 0 && static_cast<int>(result.length()) >= opts.maxChars) {
                break;
            }

            if (opts.timeoutSec >= 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - startTime).count();
                if (elapsed >= opts.timeoutSec) {
                    return 142;
                }

                DWORD avail = 0;
                if (PeekNamedPipe(hIn, NULL, 0, NULL, &avail, NULL) && avail == 0) {
                    Sleep(10);
                    continue;
                }
            }

            char c = 0;
            DWORD bytesRead = 0;
            if (!ReadFile(hIn, &c, 1, &bytesRead, NULL) || bytesRead == 0) {
                break;
            }

            wchar_t ch = static_cast<wchar_t>(c);

            if (opts.delim == L'\n' && (ch == L'\r' || ch == L'\n')) {
                if (ch == L'\r') {
                    char nextC = 0;
                    DWORD peekRead = 0;
                    if (PeekNamedPipe(hIn, &nextC, 1, &peekRead, NULL, NULL) && peekRead > 0 && nextC == '\n') {
                        ReadFile(hIn, &nextC, 1, &peekRead, NULL);
                    }
                }
                break;
            } else if (ch == opts.delim) {
                break;
            }

            if (!opts.raw && !escapeNext && ch == L'\\') {
                escapeNext = true;
                continue;
            }

            escapeNext = false;
            result.push_back(ch);
        }

        return 0;
    }
};

// ============================================================================
// 4. CORE READ ENGINE
// ============================================================================

class ReadEngine {
private:
    ReadOptions options;

public:
    explicit ReadEngine(ReadOptions opts) : options(std::move(opts)) {}

    int execute() {
        if (!options.prompt.empty()) {
            ReadReporter::writeHandle(GetStdHandle(STD_ERROR_HANDLE), options.prompt);
        }

        std::wstring result;
        int exitCode = 0;

        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            exitCode = ConsoleReader::read(options, result);
        } else {
            exitCode = PipeReader::read(options, result);
        }

        return ReadReporter::output(result, options, exitCode);
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class ReadApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        ReadOptions options;
        if (!ReadOptions::parse(argc, argv, options)) {
            return 1;
        }
        ReadEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return ReadApp::run(argc, argv);
}
