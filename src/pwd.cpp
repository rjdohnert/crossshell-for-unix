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
 * SINGLE FILE INDEX: pwd.cpp
 * ============================================================================
 * WinPwd - Object-Oriented Working Directory Resolver for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. PwdOptions class (CLI parsing, -L, -P)
 * 2. [DIRECTORY RESOLVER ENGINE] ........... PwdEngine class (logical vs physical symlink resolution)
 * 3. [APPLICATION CONTROLLER] .............. PwdApp class and wmain entry point
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

class PwdOptions {
public:
    bool physical{false};

    static void printUsage() {
        std::wcout << LR"(pwd(1)                     CrossShell for UNIX Reference Manual                    pwd(1)

    NAME
        pwd - print name of current/working directory

    SYNOPSIS
        pwd [OPTIONS]

    DESCRIPTION
        pwd prints the full pathname of the current working directory to
        standard output.

    OPTIONS
        -L, --logical
            Use PWD from the environment, even if it contains symlinks (default).

        -P, --physical
            Resolve all symlinks and directory junctions to show the physical path.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        pwd
            Print the current working directory path.

        pwd -P
            Print the physical path resolving any symbolic links or junctions.

    CrossShell for UNIX                                                          pwd(1)
)";
    }

    static void printVersion() {
        std::wcout << L"pwd 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], PwdOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage();
                std::exit(0);
            } else if (arg == L"--version") {
                printVersion();
                std::exit(0);
            } else if (arg == L"--logical") {
                opts.physical = false;
            } else if (arg == L"--physical") {
                opts.physical = true;
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    switch (arg[j]) {
                        case L'L': opts.physical = false; break;
                        case L'P': opts.physical = true; break;
                        default:
                            std::wcerr << L"pwd: unknown option -- " << arg[j] << L"\n";
                            std::wcerr << L"usage: pwd [-L | -P]\n";
                            return false;
                    }
                }
            } else {
                std::wcerr << L"pwd: too many arguments\n";
                std::wcerr << L"usage: pwd [-L | -P]\n";
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. DIRECTORY RESOLVER ENGINE
// ============================================================================

class PwdEngine {
public:
    static std::wstring stripExtendedPrefix(const std::wstring& path) {
        if (path.rfind(L"\\\\?\\UNC\\", 0) == 0) {
            return L"\\\\" + path.substr(8);
        } else if (path.rfind(L"\\\\?\\", 0) == 0) {
            return path.substr(4);
        }
        return path;
    }

    static std::wstring getLogicalCwd() {
        DWORD len = GetCurrentDirectoryW(0, nullptr);
        if (len == 0) return L"";
        std::wstring buffer(len, L'\0');
        DWORD written = GetCurrentDirectoryW(len, &buffer[0]);
        if (written > 0 && written < len) {
            buffer.resize(written);
            return buffer;
        }
        return L"";
    }

    static std::wstring getPhysicalCwd(const std::wstring& logicalPath) {
        HANDLE hDir = CreateFileW(
            logicalPath.c_str(),
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );

        if (hDir == INVALID_HANDLE_VALUE) {
            return logicalPath;
        }

        DWORD len = GetFinalPathNameByHandleW(hDir, nullptr, 0, FILE_NAME_NORMALIZED);
        if (len == 0) {
            CloseHandle(hDir);
            return logicalPath;
        }

        std::wstring buffer(len, L'\0');
        DWORD written = GetFinalPathNameByHandleW(hDir, &buffer[0], len, FILE_NAME_NORMALIZED);
        CloseHandle(hDir);

        if (written > 0 && written < len) {
            buffer.resize(written);
            return stripExtendedPrefix(buffer);
        }

        return logicalPath;
    }

    static int execute(const PwdOptions& opts) {
        std::wstring logical = getLogicalCwd();
        if (logical.empty()) {
            std::wcerr << L"pwd: failed to get current directory\n";
            return 1;
        }

        if (opts.physical) {
            std::wstring physical = getPhysicalCwd(logical);
            std::wcout << physical << L"\n";
        } else {
            std::wcout << logical << L"\n";
        }
        return 0;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class PwdApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        PwdOptions options;
        if (!PwdOptions::parse(argc, argv, options)) {
            return 1;
        }
        return PwdEngine::execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return PwdApp::run(argc, argv);
}
