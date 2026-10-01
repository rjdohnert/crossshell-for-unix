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
 * SINGLE FILE INDEX: runcon.cpp
 * ============================================================================
 * WinRuncon - Object-Oriented Process Integrity Level & Security Context Launcher
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. RunconOptions class (CLI parsing & flags)
 * 2. [TOKEN & PRIVILEGE GUARD] ............. TokenPrivilegeGuard, IntegrityLevelMapper
 * 3. [INTEGRITY PROCESS LAUNCHER] .......... IntegrityProcessLauncher class
 * 4. [APPLICATION CONTROLLER] .............. RunconApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cwctype>
#include <memory>

#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class RunconOptions {
public:
    std::wstring level;
    std::vector<std::wstring> commandArgs;

    static void printUsage(const wchar_t* progName) {
           std::wcout << LR"(runcon(1)                CrossShell for UNIX Reference Manual                   runcon(1)

    NAME
        runcon - run a command at a Windows integrity level

    SYNOPSIS
        runcon [CONTEXT] COMMAND [ARGS...]
        runcon -l LEVEL COMMAND [ARGS...]

    DESCRIPTION
        Launches COMMAND with a duplicated Windows token configured with the
        requested mandatory integrity level. Contexts may use
        [USER:][ROLE:][TYPE:][LEVEL]; only LEVEL is enforced.

    INTEGRITY LEVELS
        untrusted, 0       S-1-16-0
        low, l, 4096       S-1-16-4096
        medium, m, 8192    S-1-16-8192
        high, h, 12288     S-1-16-12288
        system, s, 16384   S-1-16-16384

    OPTIONS
        -l, --range LEVEL  Set the integrity level explicitly.
        -h, --help, /?     Display this comprehensive reference manual and exit.
        --version          Display version information and exit.
        --                 End options and introduce the command.

    EXAMPLES
        runcon low notepad.exe
        runcon -l high cmd.exe /c whoami
        runcon system service.exe -- --safe

    EXIT STATUS
        0          Help, version, or the child process's exit code.
        1          Invalid level, option, token, SID, or launch failure.

    CrossShell for UNIX                                                       runcon(1)
    )";
           return;

        std::wcout << L"Usage: " << progName << L" [CONTEXT] COMMAND [ARGS...]\n"
                   << L"  or:  " << progName << L" [ -l LEVEL ] COMMAND [ARGS...]\n\n"
                   << L"Run COMMAND with specified Windows Mandatory Integrity Level security context.\n\n"
                   << L"Context format: [USER:][ROLE:][TYPE:][LEVEL]\n"
                   << L"  (USER, ROLE, and TYPE are parsed for UNIX compatibility but LEVEL is enforced)\n\n"
                   << L"Integrity Levels:\n"
                   << L"  untrusted (0)    - Untrusted integrity level (S-1-16-0)\n"
                   << L"  low (4096)       - Low integrity level (S-1-16-4096)\n"
                   << L"  medium (8192)    - Medium integrity level (S-1-16-8192, default standard user)\n"
                   << L"  high (12288)     - High integrity level (S-1-16-12288, elevated administrator)\n"
                   << L"  system (16384)   - System integrity level (S-1-16-16384)\n\n"
                   << L"Options:\n"
                   << L"  -l, --range=LEVEL     set security level explicitly\n"
                   << L"  -h, --help            display this help and exit\n"
                   << L"      --version         output version information and exit\n";
    }

    static void printVersion() {
        std::wcout << L"runcon 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], RunconOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == L"--version") {
                printVersion();
                std::exit(0);
            } else if ((arg == L"-l" || arg == L"--range") && i + 1 < argc) {
                opts.level = argv[++i];
            } else if (arg == L"--") {
                ++i;
                break;
            } else if (arg[0] == L'-') {
                std::wcerr << L"runcon: invalid option -- '" << arg << L"'\n";
                return false;
            } else {
                break;
            }
        }

        if (opts.level.empty() && i < argc) {
            std::wstring context = argv[i++];
            size_t lastColon = context.rfind(L':');
            if (lastColon != std::wstring::npos) {
                opts.level = context.substr(lastColon + 1);
            } else {
                opts.level = context;
            }
        }

        for (; i < argc; ++i) {
            opts.commandArgs.push_back(argv[i]);
        }

        if (opts.commandArgs.empty()) {
            std::wcerr << L"runcon: you must specify a command to run\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. TOKEN & PRIVILEGE GUARD
// ============================================================================

class TokenPrivilegeGuard {
public:
    static bool enablePrivilege(LPCWSTR lpszPrivilege) {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }

        TOKEN_PRIVILEGES tp;
        LUID luid;

        if (!LookupPrivilegeValueW(NULL, lpszPrivilege, &luid)) {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) &&
                      (GetLastError() == ERROR_SUCCESS);

        CloseHandle(hToken);
        return result;
    }
};

class IntegrityLevelMapper {
public:
    static std::wstring mapLevelToSid(const std::wstring& levelStr) {
        std::wstring l = levelStr;
        for (auto& c : l) c = static_cast<wchar_t>(std::towlower(c));

        if (l == L"untrusted" || l == L"0") return L"S-1-16-0";
        if (l == L"low" || l == L"l" || l == L"4096") return L"S-1-16-4096";
        if (l == L"medium" || l == L"m" || l == L"8192") return L"S-1-16-8192";
        if (l == L"high" || l == L"h" || l == L"12288") return L"S-1-16-12288";
        if (l == L"system" || l == L"s" || l == L"16384") return L"S-1-16-16384";

        if (l.rfind(L"s-1-16-", 0) == 0) {
            return levelStr;
        }

        return L"";
    }
};

// ============================================================================
// 3. INTEGRITY PROCESS LAUNCHER
// ============================================================================

class IntegrityProcessLauncher {
private:
    static std::wstring buildCommandLine(const std::vector<std::wstring>& args) {
        std::wstring cmdLine;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmdLine += L" ";
            std::wstring arg = args[i];
            if (arg.find(L' ') != std::wstring::npos || arg.find(L'\t') != std::wstring::npos || arg.empty()) {
                cmdLine += L"\"";
                for (wchar_t c : arg) {
                    if (c == L'\"') cmdLine += L"\\\"";
                    else cmdLine += c;
                }
                cmdLine += L"\"";
            } else {
                cmdLine += arg;
            }
        }
        return cmdLine;
    }

public:
    static int launch(const std::wstring& stringSid, const std::vector<std::wstring>& args) {
        HANDLE hToken = NULL;
        HANDLE hNewToken = NULL;
        PSID pIntegritySid = NULL;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, &hToken)) {
            std::wcerr << L"runcon: Failed to open process token (Error: " << GetLastError() << L")\n";
            return 1;
        }

        if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hNewToken)) {
            std::wcerr << L"runcon: Failed to duplicate token (Error: " << GetLastError() << L")\n";
            CloseHandle(hToken);
            return 1;
        }
        CloseHandle(hToken);

        if (!ConvertStringSidToSidW(stringSid.c_str(), &pIntegritySid)) {
            std::wcerr << L"runcon: Invalid SID format or integrity string: " << stringSid << L"\n";
            CloseHandle(hNewToken);
            return 1;
        }

        TOKEN_MANDATORY_LABEL tml = { 0 };
        tml.Label.Attributes = SE_GROUP_INTEGRITY;
        tml.Label.Sid = pIntegritySid;

        if (!SetTokenInformation(hNewToken, TokenIntegrityLevel, &tml, sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pIntegritySid))) {
            std::wcerr << L"runcon: Failed to set integrity level on token (Error: " << GetLastError() << L")\n";
            LocalFree(pIntegritySid);
            CloseHandle(hNewToken);
            return 1;
        }

        LocalFree(pIntegritySid);

        std::wstring commandLine = buildCommandLine(args);
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };

        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        BOOL success = CreateProcessAsUserW(
            hNewToken,
            NULL,
            cmdBuffer.data(),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi
        );

        if (!success) {
            std::wcerr << L"runcon: Failed to execute process under selected security context (Error: " << GetLastError() << L")\n";
            CloseHandle(hNewToken);
            return 1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hNewToken);

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class RunconApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        RunconOptions options;
        if (!RunconOptions::parse(argc, argv, options)) {
            return 1;
        }

        TokenPrivilegeGuard::enablePrivilege(L"SeAssignPrimaryTokenPrivilege");
        TokenPrivilegeGuard::enablePrivilege(L"SeIncreaseQuotaPrivilege");
        TokenPrivilegeGuard::enablePrivilege(L"SeSecurityPrivilege");

        std::wstring stringSid = IntegrityLevelMapper::mapLevelToSid(options.level);
        if (stringSid.empty()) {
            std::wcerr << L"runcon: unrecognized or invalid security level: " << options.level << L"\n";
            return 1;
        }

        return IntegrityProcessLauncher::launch(stringSid, options.commandArgs);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return RunconApp::run(argc, argv);
}
