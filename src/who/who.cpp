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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>
#include <sysinfoapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <memory>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. DATA MODELS & RAII WRAPPERS
// ============================================================================

template <typename T>
class ScopedWtsMemory {
public:
    explicit ScopedWtsMemory(T* ptr = nullptr) : m_ptr(ptr) {}

    ~ScopedWtsMemory() {
        Free();
    }

    ScopedWtsMemory(const ScopedWtsMemory&) = delete;
    ScopedWtsMemory& operator=(const ScopedWtsMemory&) = delete;

    ScopedWtsMemory(ScopedWtsMemory&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedWtsMemory& operator=(ScopedWtsMemory&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    T** Receive() { Free(); return &m_ptr; }
    operator T*() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Free() {
        if (m_ptr) {
            WTSFreeMemory(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};

struct UserSession {
    std::wstring username;
    std::wstring domain;
    std::wstring line;
    std::wstring clientName;
    std::wstring logonTime;
    bool isActive = false;
};

// ============================================================================
// 2. SESSION ENUMERATION ENGINE
// ============================================================================

class SessionEnumerator {
public:
    static std::wstring FormatFileTime(const FILETIME& ft) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

        wchar_t buf[64];
        swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d",
                  stLocal.wYear, stLocal.wMonth, stLocal.wDay,
                  stLocal.wHour, stLocal.wMinute);
        return buf;
    }

    static std::wstring GetBootTime() {
        ULONGLONG uptimeMS = GetTickCount64();
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);

        ULARGE_INTEGER uli;
        uli.LowPart = ftNow.dwLowDateTime;
        uli.HighPart = ftNow.dwHighDateTime;
        uli.QuadPart -= (uptimeMS * 10000ULL);

        FILETIME ftBoot;
        ftBoot.dwLowDateTime = uli.LowPart;
        ftBoot.dwHighDateTime = uli.HighPart;

        return FormatFileTime(ftBoot);
    }

    static std::wstring GetCurrentUserName() {
        wchar_t buf[256] = { 0 };
        DWORD size = 256;
        if (GetUserNameW(buf, &size)) {
            return buf;
        }
        return L"";
    }

    static std::vector<UserSession> GetLoggedOnUsers() {
        std::vector<UserSession> sessions;
        PWTS_SESSION_INFOW pSessionInfo = nullptr;
        DWORD count = 0;

        if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
            return sessions;
        }
        ScopedWtsMemory<WTS_SESSION_INFOW> sessionListHolder(pSessionInfo);

        for (DWORD i = 0; i < count; ++i) {
            PWSTR pUserName = nullptr;
            PWSTR pDomain = nullptr;
            PWSTR pWinStation = nullptr;
            PWSTR pClientName = nullptr;
            DWORD bytesReturned = 0;

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSUserName, &pUserName, &bytesReturned);
            ScopedWtsMemory<wchar_t> userHolder(pUserName);

            if (!pUserName || wcslen(pUserName) == 0) {
                continue;
            }

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSDomainName, &pDomain, &bytesReturned);
            ScopedWtsMemory<wchar_t> domainHolder(pDomain);

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSWinStationName, &pWinStation, &bytesReturned);
            ScopedWtsMemory<wchar_t> stationHolder(pWinStation);

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSClientName, &pClientName, &bytesReturned);
            ScopedWtsMemory<wchar_t> clientHolder(pClientName);

            PWTSINFOW pInfo = nullptr;
            std::wstring timeStr = L"-";
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSSessionInfo, reinterpret_cast<PWSTR*>(&pInfo), &bytesReturned)) {
                ScopedWtsMemory<WTSINFOW> infoHolder(pInfo);
                if (pInfo && pInfo->LogonTime.QuadPart > 0) {
                    FILETIME ft;
                    ft.dwLowDateTime = pInfo->LogonTime.LowPart;
                    ft.dwHighDateTime = pInfo->LogonTime.HighPart;
                    timeStr = FormatFileTime(ft);
                }
            }

            UserSession session;
            session.username = pUserName ? pUserName : L"";
            session.domain = pDomain ? pDomain : L"";
            session.line = (pWinStation && wcslen(pWinStation) > 0) ? pWinStation : L"console";
            session.clientName = (pClientName && wcslen(pClientName) > 0) ? pClientName : L"local";
            session.logonTime = timeStr;
            session.isActive = (sessionListHolder.Get()[i].State == WTSActive);

            sessions.push_back(session);
        }

        return sessions;
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class WhoOptions {
public:
    bool optHeader = false;
    bool optCount = false;
    bool optBoot = false;
    bool optAmI = false;
    bool optAll = false;
    bool optLogin = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                break;
            } else if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            } else if (arg == L"-H" || arg == L"--heading") {
                optHeader = true;
            } else if (arg == L"-q" || arg == L"--count") {
                optCount = true;
            } else if (arg == L"-b" || arg == L"--boot") {
                optBoot = true;
            } else if (arg == L"-a" || arg == L"--all") {
                optAll = true;
            } else if (arg == L"-u" || arg == L"--login") {
                optLogin = true;
            } else if (arg == L"am" && i + 1 < argc && (_wcsicmp(argv[i + 1], L"i") == 0)) {
                optAmI = true;
                break;
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"who: invalid option '" << arg << L"'\n";
                return false;
            }
        }
        return true;
    }

    void PrintUsage(const wchar_t* progName) const {
        std::wcout << LR"(who(1)                  CrossShell for UNIX Reference Manual                   who(1)

    NAME
        who - show who is logged on to the Windows system

    SYNOPSIS
        who [OPTIONS] [am i]

    DESCRIPTION
        who prints information about users who are currently logged on to local
        or Remote Desktop / terminal sessions.

    OPTIONS
        -a, --all
            Same as -b -d --login -p -r -t -T -u.

        -b, --boot
            Time of last system boot.

        -H, --heading
            Print line of column headings.

        -q, --count
            All login names and number of users logged on.

        -u, --login
            List users logged in.

        am i, am I
            Print information about the current terminal session only.

        --json, --csv, --table
            Output user session records as JSON, CSV, or table.

        --pipe COMMAND
            Stream output to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        who -H
            List all logged on users with table headers.

        who -b
            Display last system boot time.

    CrossShell for UNIX                                                    who(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"who 1.0.0\n";
    }
};

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class WhoReporter {
public:
    static void PrintHeader() {
        std::wcout << std::left 
                  << std::setw(18) << L"NAME"
                  << std::setw(14) << L"LINE"
                  << std::setw(18) << L"TIME"
                  << L"COMMENT\n";
    }

    static void PrintSession(const UserSession& s) {
        std::wstring userDisplay = s.domain.empty() ? s.username : (s.domain + L"\\" + s.username);
        std::wstring comment = L"(" + s.clientName + L")";

        std::wcout << std::left 
                  << std::setw(18) << userDisplay
                  << std::setw(14) << s.line
                  << std::setw(18) << s.logonTime
                  << comment << L"\n";
    }

    static void PrintCount(const std::vector<UserSession>& sessions) {
        for (size_t i = 0; i < sessions.size(); ++i) {
            std::wcout << sessions[i].username << (i + 1 < sessions.size() ? L" " : L"");
        }
        std::wcout << L"\n# users=" << sessions.size() << L"\n";
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class WhoApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        WhoOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"who");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"who");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.optBoot) {
            std::wcout << L"system boot  " << SessionEnumerator::GetBootTime() << L"\n";
            return 0;
        }

        auto sessions = SessionEnumerator::GetLoggedOnUsers();

        if (opts.optAmI) {
            std::wstring currentUser = SessionEnumerator::GetCurrentUserName();
            if (opts.optHeader) WhoReporter::PrintHeader();
            for (const auto& s : sessions) {
                if (_wcsicmp(s.username.c_str(), currentUser.c_str()) == 0) {
                    WhoReporter::PrintSession(s);
                    return 0;
                }
            }
            UserSession current;
            current.username = currentUser;
            current.line = L"console";
            current.logonTime = L"-";
            current.clientName = L"local";
            WhoReporter::PrintSession(current);
            return 0;
        }

        if (opts.optCount) {
            WhoReporter::PrintCount(sessions);
            return 0;
        }

        if (opts.optHeader) WhoReporter::PrintHeader();

        for (const auto& s : sessions) {
            WhoReporter::PrintSession(s);
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    WhoApplication app;
    return app.Run(argc, argv);
}
