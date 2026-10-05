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
 * SINGLE FILE INDEX: newshell.cpp
 * ============================================================================
 * WinNewshell - Object-Oriented Terminal & Console Host Launcher for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. NewshellOptions class (CLI parsing & flags)
 * 2. [TERMINAL DISCOVERY & LAUNCHER] ....... TerminalLauncher class (wt.exe vs conhost.exe)
 * 3. [APPLICATION CONTROLLER] .............. NewshellApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class NewshellOptions {
public:
    bool runAsAdmin{false};
    bool forceConhost{false};
    std::wstring workingDir;
    std::wstring profileName;
    std::wstring customCommand;

    static void printVersion() {
        std::wcout << L"newshell 3.0.0\n"
                   << L"Copyright (C) 2026, Roberto J Dohnert\n";
    }

    static void printHelp() {
        std::wcout << L"newshell - Launch modern or legacy Windows terminals on demand.\n\n"
                   << L"USAGE:\n"
                   << L"    newshell.exe [OPTIONS] [-- <command> [args...]]\n\n"
                   << L"DESCRIPTION:\n"
                   << L"    Launches a new instance of Windows Terminal (wt.exe). If Windows\n"
                   << L"    Terminal is not installed or detected in PATH, it seamlessly falls\n"
                   << L"    back to the classic Windows Console Host (conhost.exe).\n\n"
                   << L"OPTIONS:\n"
                   << L"    -a, --admin          Prompt for UAC elevation to launch an Administrator terminal.\n"
                   << L"    -d, --dir <PATH>     Set the initial working directory for the terminal session.\n"
                   << L"    -p, --profile <NAME> Specify a Windows Terminal profile (ignored in conhost fallback).\n"
                   << L"    -f, --force-conhost  Force classic Console Host (conhost.exe) instead of Windows Terminal.\n"
                   << L"    -h, --help           Display this comprehensive help menu and exit.\n"
                   << L"    -v, --version        Display version and copyright information and exit.\n\n"
                   << L"EXAMPLES:\n"
                   << L"    newshell\n"
                   << L"        Opens a new Windows Terminal (or conhost fallback) in the current directory.\n\n"
                   << L"    newshell --admin\n"
                   << L"        Triggers a UAC prompt and opens an elevated Administrator terminal.\n\n"
                   << L"    newshell --dir \"C:\\Projects\" --admin\n"
                   << L"        Opens an elevated terminal rooted at C:\\Projects.\n\n"
                   << L"    newshell --force-conhost --admin\n"
                   << L"        Forces classic elevated conhost.exe session.\n\n"
                   << L"    newshell -- ping 1.1.1.1 -t\n"
                   << L"        Launches a new terminal executing the specified command.\n";
    }

    static bool parse(int argc, wchar_t* argv[], NewshellOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"-v" || arg == L"--version") {
                printVersion();
                std::exit(0);
            } else if (arg == L"-a" || arg == L"--admin") {
                opts.runAsAdmin = true;
            } else if (arg == L"-f" || arg == L"--force-conhost") {
                opts.forceConhost = true;
            } else if ((arg == L"-d" || arg == L"--dir") && i + 1 < argc) {
                opts.workingDir = argv[++i];
            } else if ((arg == L"-p" || arg == L"--profile") && i + 1 < argc) {
                opts.profileName = argv[++i];
            } else if (arg == L"--") {
                for (int j = i + 1; j < argc; ++j) {
                    if (!opts.customCommand.empty()) opts.customCommand += L" ";
                    std::wstring cmdPart = argv[j];
                    if (cmdPart.find(L' ') != std::wstring::npos) {
                        opts.customCommand += L"\"" + cmdPart + L"\"";
                    } else {
                        opts.customCommand += cmdPart;
                    }
                }
                break;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. TERMINAL DISCOVERY & LAUNCHER
// ============================================================================

class TerminalLauncher {
public:
    static bool isWindowsTerminalAvailable() {
        DWORD bufferLen = SearchPathW(nullptr, L"wt.exe", nullptr, 0, nullptr, nullptr);
        return bufferLen > 0;
    }

    static bool launch(const std::wstring& targetExe,
                       const std::wstring& targetArgs,
                       const std::wstring& workingDir,
                       bool runAsAdmin) {
        SHELLEXECUTEINFOW sei = { sizeof(SHELLEXECUTEINFOW) };
        sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
        sei.lpVerb = runAsAdmin ? L"runas" : L"open";
        sei.lpFile = targetExe.c_str();
        sei.lpParameters = targetArgs.empty() ? nullptr : targetArgs.c_str();
        sei.lpDirectory = workingDir.empty() ? nullptr : workingDir.c_str();
        sei.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&sei)) {
            DWORD error = GetLastError();
            if (error == ERROR_CANCELLED) {
                std::wcerr << L"[newshell] Error: Elevation request canceled by user.\n";
            } else {
                std::wcerr << L"[newshell] Error: Failed to launch process. Windows error code: " << error << L"\n";
            }
            return false;
        }
        return true;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class NewshellApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        NewshellOptions options;
        if (!NewshellOptions::parse(argc, argv, options)) {
            return 1;
        }

        if (!options.workingDir.empty()) {
            DWORD attributes = GetFileAttributesW(options.workingDir.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wcerr << L"[newshell] Warning: Working directory does not exist. Using current directory.\n";
                options.workingDir = L"";
            } else {
                DWORD required = GetFullPathNameW(options.workingDir.c_str(), 0, nullptr, nullptr);
                if (required > 0) {
                    std::vector<wchar_t> fullPath(required);
                    DWORD written = GetFullPathNameW(options.workingDir.c_str(), required, fullPath.data(), nullptr);
                    if (written > 0 && written < required) {
                        options.workingDir.assign(fullPath.data(), written);
                    }
                }
            }
        }

        std::wstring targetExe;
        std::wstring targetArgs;

        if (!options.forceConhost && TerminalLauncher::isWindowsTerminalAvailable()) {
            targetExe = L"wt.exe";
            if (!options.profileName.empty()) {
                targetArgs += L"-p \"" + options.profileName + L"\" ";
            }
            if (!options.workingDir.empty()) {
                targetArgs += L"-d \"" + options.workingDir + L"\" ";
            }
            if (!options.customCommand.empty()) {
                targetArgs += options.customCommand;
            }
        } else {
            targetExe = L"conhost.exe";
            if (!options.customCommand.empty()) {
                targetArgs = L"cmd.exe /k " + options.customCommand;
            } else {
                targetArgs = L"cmd.exe";
            }
        }

        bool success = TerminalLauncher::launch(targetExe, targetArgs, options.workingDir, options.runAsAdmin);
        return success ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return NewshellApp::run(argc, argv);
}