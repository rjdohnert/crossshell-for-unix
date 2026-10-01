/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <userenv.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "userenv.lib")

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Formats Win32 Error Codes into human-readable strings
std::wstring GetSystemErrorMessage(DWORD errorCode) {
    LPWSTR buf = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&buf, 0, NULL
    );
    std::wstring msg = (size && buf) ? buf : L"Unknown error.";
    if (buf) LocalFree(buf);

    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n')) {
        msg.pop_back();
    }
    return msg;
}

// Securely reads password from terminal without echoing characters
std::wstring ReadPassword(const std::wstring& prompt) {
    std::wcout << prompt;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);
    
    // Disable character echo
    SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);

    std::wstring password;
    std::getline(std::wcin, password);

    // Restore console mode
    SetConsoleMode(hStdin, mode);
    std::wcout << L"\n";
    return password;
}

// Parses user input into domain and username components
void ParseUserAndDomain(const std::wstring& inputUser, std::wstring& username, std::wstring& domain) {
    size_t slashPos = inputUser.find(L'\\');
    size_t atPos = inputUser.find(L'@');

    if (slashPos != std::wstring::npos) {
        domain = inputUser.substr(0, slashPos);
        username = inputUser.substr(slashPos + 1);
    } else if (atPos != std::wstring::npos) {
        username = inputUser.substr(0, atPos);
        domain = inputUser.substr(atPos + 1);
    } else {
        username = inputUser;
        domain = L"."; // Local machine
    }
}

std::wstring QuoteForCommandLine(const std::wstring& value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'\"') {
            quoted += L"\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring BuildShellCommand(const std::wstring& customCommand, bool loginShell) {
    if (!customCommand.empty()) {
        std::wstring shell = loginShell ? L"ksh.exe -l -c " : L"ksh.exe -c ";
        return shell + QuoteForCommandLine(customCommand);
    }
    return loginShell ? L"ksh.exe -l" : L"ksh.exe";
}

// ============================================================================
// MAIN PROCESS SPAWNER (IN-TERMINAL EXECUTION)
// ============================================================================
DWORD ExecuteInSameTerminal(
    const std::wstring& username,
    const std::wstring& domain,
    const std::wstring& password,
    const std::wstring& commandLine) 
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    wchar_t cmdBuf[32768];
    wcscpy_s(cmdBuf, commandLine.c_str());

    // Launch the target user process via Windows Terminal.
    BOOL ok = CreateProcessWithLogonW(
        username.c_str(),
        domain.c_str(),
        password.c_str(),
        LOGON_WITH_PROFILE,
        NULL,
        cmdBuf,
        CREATE_UNICODE_ENVIRONMENT,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_LOGON_FAILURE) {
            std::wcerr << L"su: Authentication failure\n";
        } else if (err == ERROR_ACCOUNT_DISABLED) {
            std::wcerr << L"su: Account disabled\n";
        } else {
            std::wcerr << L"su: " << GetSystemErrorMessage(err) << L"\n";
        }
        return err;
    }

    // Block calling process and wait for subshell to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
}

// ============================================================================
// CLI PARSER & ENTRY POINT
// ============================================================================
void ShowHelp() {
    std::wcout << LR"(su(1)                   CrossShell for UNIX Reference Manual                 su(1)

    NAME
        su - run a shell with substitute user and domain credentials

    SYNOPSIS
        su [OPTIONS] [USER]

    DESCRIPTION
        Switches to another user account or executes commands under substitute
        credentials using the Windows Logon API (CreateProcessWithLogonW).
        Prompts securely for account password without echoing. Default USER is
        'Administrator'.

    OPTIONS
        -l, -, --login
            Start the shell as a login shell.

        -c, --command COMMAND
            Pass COMMAND string to the invoked shell.

        -h, --help
            Display this reference manual.

    EXAMPLES
        su
            Switch to local Administrator with an interactive shell.

        su -l Developer
            Start a login shell as the Developer user.

        su -c "whoami /all" AdminUser
            Execute whoami command under the AdminUser credentials.

    CrossShell for UNIX                                                    su(1)
    )";
}

int wmain(int argc, wchar_t* argv[]) {
    std::wstring targetUser = L"Administrator";
    std::wstring customCommand = L"";
    bool loginShell = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--help" || arg == L"-h" || arg == L"-?" || arg == L"/?") {
            ShowHelp();
            return 0;
        } else if (arg == L"-" || arg == L"-l" || arg == L"--login") {
            loginShell = true;
        } else if ((arg == L"-c" || arg == L"--command") && i + 1 < argc) {
            customCommand = argv[++i];
        } else if (arg[0] != L'-') {
            targetUser = arg;
        }
    }

    std::wstring username, domain;
    ParseUserAndDomain(targetUser, username, domain);

    // Prompt for target account password
    std::wstring prompt = L"Password for " + targetUser + L": ";
    std::wstring password = ReadPassword(prompt);

    // Determine target execution binary inside Windows Terminal.
    std::wstring shellCommand = BuildShellCommand(customCommand, loginShell);
    std::wstring commandLine = L"wt.exe new-tab " + shellCommand;

    DWORD result = ExecuteInSameTerminal(username, domain, password, commandLine);
    return (result == 0) ? 0 : 1;
}
