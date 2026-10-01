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
 * SINGLE FILE INDEX: printenv.cpp
 * ============================================================================
 * WinPrintenv - Object-Oriented Environment Variable Inspector for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. PrintenvOptions class (CLI parsing & flags)
 * 2. [TERMINAL FORMATTER] .................. OutputFormatter class (console modes, path columns)
 * 3. [ENVIRONMENT INSPECTOR ENGINE] ........ PrintenvEngine class (environment block traversal)
 * 4. [APPLICATION CONTROLLER] .............. PrintenvApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cwctype>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class PrintenvOptions {
public:
    bool nullTerminated{false};
    std::vector<std::wstring> targets;

    static void printHelp() {
        std::wcout << LR"(printenv(1)                CrossShell for UNIX Reference Manual                printenv(1)

    NAME
        printenv - print all or part of environment

    SYNOPSIS
        printenv [OPTIONS] [VARIABLE...]

    DESCRIPTION
        printenv prints the values of the specified environment VARIABLE(s).
        If no VARIABLE is specified, it prints name and value pairs for all
        environment variables in the current process environment.

    OPTIONS
        -0, --null
            End each output line with NUL (0 byte) rather than newline.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

        --
            End of option processing. Subsequent arguments are treated as
            variable names even if they begin with a dash.

    EXAMPLES
        printenv
            Print all environment variables as KEY=VALUE lines.

        printenv PATH
            Print the value of the PATH environment variable.

        printenv USER PROFILE
            Print values of USER and PROFILE variables sequentially.

        printenv -0 PATH
            Print PATH terminated by a null character for scripting.

    CrossShell for UNIX                                                          printenv(1)
)";
    }

    static void printVersion() {
        std::wcout << L"printenv 1.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], PrintenvOptions& opts) {
        bool parseOptions = true;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (parseOptions && arg == L"--") {
                parseOptions = false;
                continue;
            }

            if (parseOptions && (arg == L"-0" || arg == L"--null")) {
                opts.nullTerminated = true;
            } else if (parseOptions && (arg == L"-h" || arg == L"--help" || arg == L"/?")) {
                printHelp();
                std::exit(0);
            } else if (parseOptions && (arg == L"-v" || arg == L"--version")) {
                printVersion();
                std::exit(0);
            } else if (parseOptions && arg[0] == L'-' && arg.size() > 1) {
                std::wstring opt = arg.substr(1);
                if (opt == L"-null") {
                    opts.nullTerminated = true;
                } else if (opt == L"-help") {
                    printHelp();
                    std::exit(0);
                } else if (opt == L"-version") {
                    printVersion();
                    std::exit(0);
                } else {
                    std::wcerr << L"printenv: unrecognized option: " << arg << L"\n";
                    printHelp();
                    return false;
                }
            } else {
                opts.targets.push_back(arg);
            }
        }
        return true;
    }
};

// ============================================================================
// 2. TERMINAL FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static bool isConsoleFd(int fd) {
        if (!_isatty(fd)) return false;
        HANDLE h = (fd == _fileno(stdout)) ? GetStdHandle(STD_OUTPUT_HANDLE) : GetStdHandle(STD_ERROR_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == NULL) return false;
        DWORD mode = 0;
        return GetConsoleMode(h, &mode) != 0;
    }

    static void configureMode() {
        const int outFd = _fileno(stdout);
        const int errFd = _fileno(stderr);

        if (isConsoleFd(outFd)) {
            _setmode(outFd, _O_U16TEXT);
        } else {
            _setmode(outFd, _O_U8TEXT);
        }

        if (isConsoleFd(errFd)) {
            _setmode(errFd, _O_U16TEXT);
        } else {
            _setmode(errFd, _O_U8TEXT);
        }
    }

    static std::wstring sanitize(const std::wstring& input) {
        std::wostringstream out;
        out << std::uppercase;

        for (wchar_t ch : input) {
            switch (ch) {
                case L'\r': out << L"\\r"; break;
                case L'\n': out << L"\\n"; break;
                case L'\t': out << L"\\t"; break;
                default:
                    if (ch < 32 || ch == 127) {
                        out << L"\\x"
                            << std::hex << std::setw(2) << std::setfill(L'0')
                            << static_cast<unsigned int>(ch)
                            << std::dec;
                    } else {
                        out << ch;
                    }
                    break;
            }
        }
        return out.str();
    }

    static bool iequals(const std::wstring& a, const std::wstring& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::towlower(a[i]) != std::towlower(b[i])) return false;
        }
        return true;
    }

    static bool isPathVariable(const std::wstring& name) {
        return iequals(name, L"PATH");
    }

    static std::vector<std::wstring> splitPath(const std::wstring& value) {
        std::vector<std::wstring> entries;
        size_t start = 0;
        while (start <= value.size()) {
            size_t sep = value.find(L';', start);
            if (sep == std::wstring::npos) {
                entries.push_back(value.substr(start));
                break;
            }
            entries.push_back(value.substr(start, sep - start));
            start = sep + 1;
        }
        return entries;
    }

    static void printPathColumns(const std::wstring& name, const std::wstring& value, bool includeName) {
        if (includeName) {
            std::wcout << name << L"=";
        }
        std::wcout.put(L'\n');

        std::vector<std::wstring> entries = splitPath(value);
        for (const auto& entry : entries) {
            std::wcout << L"  " << sanitize(entry) << L'\n';
        }
    }
};

// ============================================================================
// 3. ENVIRONMENT INSPECTOR ENGINE
// ============================================================================

class PrintenvEngine {
public:
    static std::wstring getEnvVar(const std::wstring& name, bool& exists) {
        exists = false;
        DWORD bufferSize = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
        if (bufferSize == 0) {
            if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
                exists = false;
                return L"";
            }
            exists = true;
            return L"";
        }

        std::wstring val(bufferSize, L'\0');
        DWORD copied = GetEnvironmentVariableW(name.c_str(), &val[0], bufferSize);
        if (copied == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            exists = false;
            return L"";
        }

        if (copied >= bufferSize) {
            val.resize(copied);
            copied = GetEnvironmentVariableW(name.c_str(), &val[0], copied);
        }

        exists = true;
        if (copied > 0) {
            val.resize(copied);
        } else {
            val.clear();
        }
        return val;
    }

    static int execute(const PrintenvOptions& opts) {
        OutputFormatter::configureMode();

        // Case 1: Print all environment variables
        if (opts.targets.empty()) {
            LPWCH envBlock = GetEnvironmentStringsW();
            if (envBlock == nullptr) {
                std::wcerr << L"printenv: failed to retrieve environment block\n";
                return 1;
            }

            LPWCH current = envBlock;
            while (*current != L'\0') {
                std::wstring envStr(current);

                // Skip hidden Windows internal command variables
                if (!envStr.empty() && envStr[0] != L'=') {
                    if (opts.nullTerminated) {
                        std::wcout << envStr;
                        std::wcout.put(L'\0');
                    } else {
                        size_t eq = envStr.find(L'=');
                        if (eq != std::wstring::npos) {
                            std::wstring name = envStr.substr(0, eq);
                            std::wstring value = envStr.substr(eq + 1);
                            if (OutputFormatter::isPathVariable(name) && OutputFormatter::isConsoleFd(_fileno(stdout))) {
                                OutputFormatter::printPathColumns(name, value, true);
                                current += envStr.length() + 1;
                                continue;
                            }
                        }
                        std::wcout << OutputFormatter::sanitize(envStr);
                        std::wcout.put(L'\n');
                    }
                }
                current += envStr.length() + 1;
            }

            FreeEnvironmentStringsW(envBlock);
            std::wcout.flush();
            return 0;
        }

        // Case 2: Specific environment variables requested
        bool allFound = true;
        for (const auto& target : opts.targets) {
            bool exists = false;
            std::wstring val = getEnvVar(target, exists);
            if (exists) {
                if (opts.nullTerminated) {
                    std::wcout << val;
                    std::wcout.put(L'\0');
                } else {
                    if (OutputFormatter::isPathVariable(target) && OutputFormatter::isConsoleFd(_fileno(stdout))) {
                        OutputFormatter::printPathColumns(target, val, false);
                    } else {
                        std::wcout << OutputFormatter::sanitize(val);
                        std::wcout.put(L'\n');
                    }
                }
            } else {
                allFound = false;
            }
        }

        std::wcout.flush();
        return allFound ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class PrintenvApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        PrintenvOptions options;
        if (!PrintenvOptions::parse(argc, argv, options)) {
            return 1;
        }
        return PrintenvEngine::execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return PrintenvApp::run(argc, argv);
}
