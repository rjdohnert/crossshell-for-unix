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
 * SINGLE FILE INDEX: pkg.cpp
 * ============================================================================
 * WinPkg - Object-Oriented BSD-Style Package Management Frontend for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & ARGUMENT ESCAPER] .......... PkgOptions, ArgumentEscaper classes
 * 2. [PACKAGE MANAGER BRIDGE] .............. LocalPackageInstaller, WingetBridge classes
 * 3. [APPLICATION CONTROLLER] .............. PkgApp class and wmain entry point
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cwctype>
#include <memory>

// ============================================================================
// 1. OPTIONS & ARGUMENT ESCAPER
// ============================================================================

class ArgumentEscaper {
public:
    static std::wstring escape(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find(L' ') == std::wstring::npos &&
            arg.find(L'\t') == std::wstring::npos &&
            arg.find(L'"') == std::wstring::npos) {
            return arg;
        }

        std::wstring escaped = L"\"";
        for (wchar_t c : arg) {
            if (c == L'"') {
                escaped += L"\\\"";
            } else {
                escaped += c;
            }
        }
        escaped += L"\"";
        return escaped;
    }

    static std::wstring toLower(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return value;
    }

    static bool endsWithCi(const std::wstring& value, const std::wstring& suffix) {
        if (value.size() < suffix.size()) return false;
        return toLower(value.substr(value.size() - suffix.size())) == toLower(suffix);
    }
};


class PkgOptions {
public:
    std::wstring command;
    std::vector<std::wstring> arguments;

    static void printHelp() {
        std::wcout << LR"(pkg(8)                  CrossShell for UNIX Reference Manual                  pkg(8)

    NAME
        pkg - package management utility for installing and managing applications

    SYNOPSIS
        pkg COMMAND [OPTIONS] [ARGUMENTS...]
        pkg [OPTIONS]

    DESCRIPTION
        pkg provides a unified BSD-style package management interface on Windows.
        It delegates package installation, search, querying, and upgrading to the
        Windows Package Manager (winget) subsystem while supporting direct local
        installation for MSI and MSIX software distributions.

    COMMANDS
        install, add PACKAGE...
            Install one or more packages by name or identifier.

        install --local FILE
            Install a local Windows installer package (.msi or .msix).

        delete, remove, rm PACKAGE...
            Uninstall one or more installed packages.

        search QUERY
            Search package repositories for applications matching QUERY.

        upgrade [PACKAGE...]
            Upgrade specified packages or all installed packages if none is given.

        info, list [PACKAGE]
            Display details for PACKAGE or enumerate all installed applications.

        update
            Update package manager repository sources and metadata catalogs.

        help
            Display this reference manual.

    OPTIONS
        --local FILE
            Install a local package file (.msi or .msix) directly.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        pkg install Git.Git
            Install Git using the Windows Package Manager repository.

        pkg install --local package.msix
            Install a local MSIX application package.

        pkg search python
            Search for available Python packages.

        pkg upgrade
            Upgrade all installed packages to their latest available versions.

        pkg delete Neovim.Neovim
            Uninstall Neovim.

        pkg info
            List all currently installed application packages.

    CrossShell for UNIX                                                   pkg(8)
)";
    }

    static void printVersion() {
        std::wcout << L"pkg (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
    }

    static bool parse(int argc, wchar_t* argv[], PkgOptions& opts, bool& showHelp, bool& showVersion) {
        showHelp = false;
        showVersion = false;

        if (argc < 2) {
            showHelp = true;
            return false;
        }

        opts.command = argv[1];
        int start = 2;

        if (opts.command == L"--") {
            if (argc < 3) {
                showHelp = true;
                return false;
            }
            opts.command = argv[2];
            start = 3;
        }

        if (opts.command == L"help" || opts.command == L"-h" || opts.command == L"--help" || opts.command == L"/?" || opts.command == L"-?") {
            showHelp = true;
            return true;
        }

        if (opts.command == L"-V" || opts.command == L"-v" || opts.command == L"--version") {
            showVersion = true;
            return true;
        }

        for (int i = start; i < argc; ++i) {
            opts.arguments.push_back(argv[i]);
        }

        return true;
    }
};

// ============================================================================
// 2. PACKAGE MANAGER BRIDGE
// ============================================================================

class LocalPackageInstaller {
public:
    static int installLocal(const std::wstring& packagePath) {
        std::wstring cmd;
        if (ArgumentEscaper::endsWithCi(packagePath, L".msi")) {
            cmd = L"msiexec /i " + ArgumentEscaper::escape(packagePath);
        } else if (ArgumentEscaper::endsWithCi(packagePath, L".msix")) {
            cmd = L"powershell -NoProfile -ExecutionPolicy Bypass -Command Add-AppxPackage -Path " + ArgumentEscaper::escape(packagePath);
        } else {
            std::wcerr << L"pkg: --local only supports .msi and .msix files\n";
            return 1;
        }

        return _wsystem(cmd.c_str());
    }
};

class WingetBridge {
public:
    static int execute(const PkgOptions& options) {
        std::wstring cmd = options.command;
        const auto& args = options.arguments;

        if (cmd == L"install" || cmd == L"add") {
            bool localInstall = false;
            std::vector<std::wstring> filtered;

            for (const auto& a : args) {
                if (a == L"--local") localInstall = true;
                else filtered.push_back(a);
            }

            if (localInstall) {
                if (filtered.size() != 1) {
                    std::wcerr << L"pkg: --local expects exactly one file path (.msi or .msix)\n";
                    return 1;
                }
                return LocalPackageInstaller::installLocal(filtered[0]);
            }

            if (filtered.empty()) {
                std::wcerr << L"pkg: missing package name\n";
                return 1;
            }

            std::wstring full = L"winget install";
            for (const auto& a : filtered) full += L" " + ArgumentEscaper::escape(a);
            return _wsystem(full.c_str());
        }

        if (cmd == L"delete" || cmd == L"remove" || cmd == L"rm") {
            if (args.empty()) {
                std::wcerr << L"pkg: missing package name\n";
                return 1;
            }
            std::wstring full = L"winget uninstall";
            for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
            return _wsystem(full.c_str());
        }

        if (cmd == L"search") {
            if (args.empty()) {
                std::wcerr << L"pkg: missing search query\n";
                return 1;
            }
            std::wstring full = L"winget search";
            for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
            return _wsystem(full.c_str());
        }

        if (cmd == L"upgrade") {
            std::wstring full = args.empty() ? L"winget upgrade --all" : L"winget upgrade";
            for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
            return _wsystem(full.c_str());
        }

        if (cmd == L"info" || cmd == L"list") {
            std::wstring full = args.empty() ? L"winget list" : L"winget show";
            for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
            return _wsystem(full.c_str());
        }

        if (cmd == L"update") {
            return _wsystem(L"winget source update");
        }

        std::wcerr << L"Unknown command: " << cmd << L"\n\n";
        PkgOptions::printHelp();
        return 1;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class PkgApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        PkgOptions options;
        bool showHelp = false;
        bool showVersion = false;

        if (!PkgOptions::parse(argc, argv, options, showHelp, showVersion)) {
            PkgOptions::printHelp();
            return 1;
        }

        if (showHelp) {
            PkgOptions::printHelp();
            return 0;
        }

        if (showVersion) {
            PkgOptions::printVersion();
            return 0;
        }

        return WingetBridge::execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return PkgApp::run(argc, argv);
}