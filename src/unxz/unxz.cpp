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
 * SINGLE FILE INDEX: unxz.cpp
 * ============================================================================
 * WinUnxz - Object-Oriented XZ Decompression Frontend Dispatcher for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & BACKEND CONFIG] ............ UnxzBackendConfig, UnxzOptions classes
 * 2. [BACKEND DISCOVERY & BRIDGE] .......... UnxzBackendFinder, UnxzProcessRunner classes
 * 3. [APPLICATION CONTROLLER] .............. UnxzApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. OPTIONS & BACKEND CONFIG
// ============================================================================

struct UnxzBackendConfig {
    std::wstring applicationPath;
    std::vector<std::wstring> fixedArgs;
};

class UnxzOptions {
public:
    std::vector<std::wstring> forwardedArgs;

    static void printHelp() {
        std::wcout << LR"(unxz(1)                 CrossShell for UNIX Reference Manual                 unxz(1)

    NAME
        unxz - decompress .xz files

    SYNOPSIS
        unxz [OPTIONS] [FILE...]

    DESCRIPTION
        Decompress FILEs in the .xz format.
        This frontend transparently delegates execution to an installed backend
        such as native xz (xz -d) or 7-Zip (7z x).
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -k, --keep
            Keep (do not delete) input files.

        -f, --force
            Force overwrite of output files.

        -h, --help
            Display this reference manual and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        unxz archive.tar.xz
            Decompress archive.tar.xz to archive.tar.

        unxz -k data.xz
            Decompress data.xz while keeping the original .xz file.

    CrossShell for UNIX                                                      unxz(1)
)";
    }

    static void printVersion(const UnxzBackendConfig& backend) {
        std::wcout << L"unxz 1.0.0\n";
        if (!backend.applicationPath.empty()) {
            std::wcout << L"Active backend: " << backend.applicationPath << L"\n";
        } else {
            std::wcout << L"No backend detected in PATH or standard installation folders.\n";
        }
    }

    static bool parse(int argc, wchar_t* argv[], UnxzOptions& opts, bool& showHelp, bool& showVersion) {
        showHelp = false;
        showVersion = false;

        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i] ? argv[i] : L"";
            if (a == L"--help" || a == L"-h" || a == L"/?" || a == L"-?") {
                showHelp = true;
                return true;
            }
            if (a == L"--version" || a == L"-V" || a == L"-v") {
                showVersion = true;
                return true;
            }
            opts.forwardedArgs.push_back(a);
        }
        return true;
    }
};

// ============================================================================
// 2. BACKEND DISCOVERY & BRIDGE
// ============================================================================

class UnxzBackendFinder {
private:
    static std::wstring toLower(const std::wstring& s) {
        std::wstring out = s;
        std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return out;
    }

    static std::wstring normalizePath(const std::wstring& p) {
        std::error_code ec;
        fs::path fp(p);
        fs::path absPath = fs::absolute(fp, ec);
        if (!ec) {
            fs::path canon = fs::weakly_canonical(absPath, ec);
            if (!ec) return toLower(canon.wstring());
            return toLower(absPath.wstring());
        }
        return toLower(fp.wstring());
    }

    static std::wstring findInPath(const std::wstring& exe) {
        wchar_t buf[MAX_PATH];
        DWORD len = SearchPathW(nullptr, exe.c_str(), nullptr, MAX_PATH, buf, nullptr);
        if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
        return L"";
    }

    static std::wstring selfExePath() {
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) return std::wstring(buf, len);
        return L"";
    }

public:
    static UnxzBackendConfig discoverBackend() {
        std::wstring self = normalizePath(selfExePath());

        // Check for native xz.exe
        std::wstring xzPath = findInPath(L"xz.exe");
        if (!xzPath.empty() && normalizePath(xzPath) != self) {
            return { xzPath, { L"-d" } };
        }

        // Check 7-Zip in PATH and standard locations
        std::vector<std::wstring> sevenZipCandidates = {
            L"7z.exe", L"7za.exe",
            L"C:\\Program Files\\7-Zip\\7z.exe",
            L"C:\\Program Files (x86)\\7-Zip\\7z.exe"
        };

        for (const auto& cand : sevenZipCandidates) {
            std::wstring found = findInPath(cand);
            if (found.empty()) {
                std::error_code ec;
                if (fs::exists(cand, ec)) found = cand;
            }
            if (!found.empty() && normalizePath(found) != self) {
                return { found, { L"x" } };
            }
        }

        return {};
    }
};

class UnxzProcessRunner {
private:
    static std::wstring quoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

public:
    static int execute(const UnxzBackendConfig& backend, const std::vector<std::wstring>& userArgs) {
        std::vector<std::wstring> fullArgs;
        fullArgs.push_back(backend.applicationPath);
        for (const auto& a : backend.fixedArgs) fullArgs.push_back(a);
        for (const auto& a : userArgs) fullArgs.push_back(a);

        std::wstring cmdline;
        for (size_t i = 0; i < fullArgs.size(); ++i) {
            if (i > 0) cmdline.push_back(L' ');
            cmdline += quoteArgument(fullArgs[i]);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
        if (!ok) {
            std::wcerr << L"unxz: failed to execute backend: " << backend.applicationPath << L"\n";
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class UnxzApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        UnxzOptions options;
        bool showHelp = false;
        bool showVersion = false;

        if (!UnxzOptions::parse(argc, argv, options, showHelp, showVersion)) {
            return 1;
        }

        if (showHelp) {
            UnxzOptions::printHelp();
            return 0;
        }

        UnxzBackendConfig backend = UnxzBackendFinder::discoverBackend();

        if (showVersion) {
            UnxzOptions::printVersion(backend);
            return 0;
        }

        if (backend.applicationPath.empty()) {
            std::wcerr << L"unxz: no suitable backend found (requires xz.exe or 7z.exe in PATH)\n";
            return 127;
        }

        return UnxzProcessRunner::execute(backend, options.forwardedArgs);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return UnxzApp::run(argc, argv);
}
