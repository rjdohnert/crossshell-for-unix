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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <lm.h>
#include <wtsapi32.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <memory>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. RAII WRAPPERS & SCOPES
// ============================================================================

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET sock = INVALID_SOCKET) : m_socket(sock) {}

    ~ScopedSocket() {
        Close();
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
        other.m_socket = INVALID_SOCKET;
    }

    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

    bool IsValid() const { return m_socket != INVALID_SOCKET; }
    SOCKET Get() const { return m_socket; }
    operator SOCKET() const { return m_socket; }

    void Close() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void Reset(SOCKET sock = INVALID_SOCKET) {
        Close();
        m_socket = sock;
    }

private:
    SOCKET m_socket;
};

template <typename T>
class ScopedWtsMemory {
public:
    explicit ScopedWtsMemory(T* ptr = nullptr) : m_ptr(ptr) {}
    ~ScopedWtsMemory() { Free(); }

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

template <typename T>
class ScopedNetApiMemory {
public:
    explicit ScopedNetApiMemory(T* ptr = nullptr) : m_ptr(ptr) {}
    ~ScopedNetApiMemory() { Free(); }

    ScopedNetApiMemory(const ScopedNetApiMemory&) = delete;
    ScopedNetApiMemory& operator=(const ScopedNetApiMemory&) = delete;

    ScopedNetApiMemory(ScopedNetApiMemory&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedNetApiMemory& operator=(ScopedNetApiMemory&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    operator T*() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Free() {
        if (m_ptr) {
            NetApiBufferFree(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};

// ============================================================================
// 2. DATA MODELS
// ============================================================================

struct PinkySessionSummary {
    std::wstring username;
    std::wstring fullName;
    std::wstring line;
    std::wstring idle;
    std::wstring logonTime;
    std::wstring host;
};

struct PinkyUserProfile {
    std::wstring username;
    std::wstring fullName;
    std::wstring homeDir;
    std::wstring comment;
    std::wstring shell;
    DWORD lastLogon = 0;
    bool userFound = false;
    std::vector<std::wstring> planLines;
};

// ============================================================================
// 3. REMOTE FINGER CLIENT & LOCAL PROFILE ENGINE
// ============================================================================

class FingerClient {
public:
    static bool QueryRemote(const std::string& user, const std::string& host) {
        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "pinky: Winsock initialization failed\n";
            return false;
        }

        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        if (getaddrinfo(host.c_str(), "79", &hints, &result) != 0) {
            std::cerr << "pinky: unknown host: " << host << "\n";
            return false;
        }

        ScopedSocket sock;
        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            SOCKET s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (s == INVALID_SOCKET) continue;

            if (connect(s, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == SOCKET_ERROR) {
                closesocket(s);
                continue;
            }

            sock.Reset(s);
            break;
        }

        freeaddrinfo(result);

        if (!sock.IsValid()) {
            std::cerr << "pinky: connect failed to " << host << " on port 79\n";
            return false;
        }

        std::string request = user + "\r\n";
        send(sock.Get(), request.c_str(), static_cast<int>(request.length()), 0);

        char buffer[512];
        int bytesReceived = 0;
        while ((bytesReceived = recv(sock.Get(), buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0';
            std::cout << buffer;
        }

        return true;
    }
};

class PinkyProfileService {
public:
    static std::wstring FormatTime(DWORD timeSecs) {
        if (timeSecs == 0) return L"-";
        time_t t = static_cast<time_t>(timeSecs);
        tm tmLocal;
        localtime_s(&tmLocal, &t);
        wchar_t buf[64];
        wcsftime(buf, sizeof(buf) / sizeof(wchar_t), L"%b %d %H:%M", &tmLocal);
        return buf;
    }

    static std::string WideToUtf8(const std::wstring& value) {
        if (value.empty()) return {};
        int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 1) return {};
        std::string utf8(static_cast<size_t>(required - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &utf8[0], required, nullptr, nullptr);
        return utf8;
    }

    static std::wstring GetUserFullName(const std::wstring& username) {
        LPUSER_INFO_10 pBuf = nullptr;
        if (NetUserGetInfo(nullptr, username.c_str(), 10, reinterpret_cast<LPBYTE*>(&pBuf)) == NERR_Success && pBuf) {
            ScopedNetApiMemory<USER_INFO_10> bufHolder(pBuf);
            if (pBuf->usri10_full_name && wcslen(pBuf->usri10_full_name) > 0) {
                return pBuf->usri10_full_name;
            }
        }
        return username;
    }

    static std::vector<PinkySessionSummary> QueryActiveSessions() {
        std::vector<PinkySessionSummary> list;
        PWTS_SESSION_INFOW pSessionInfo = nullptr;
        DWORD count = 0;

        if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
            return list;
        }
        ScopedWtsMemory<WTS_SESSION_INFOW> sessionHolder(pSessionInfo);

        for (DWORD i = 0; i < count; ++i) {
            PWSTR pUserName = nullptr;
            PWSTR pWinStation = nullptr;
            PWSTR pClientName = nullptr;
            DWORD bytes = 0;

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSUserName, &pUserName, &bytes);
            ScopedWtsMemory<wchar_t> userHolder(pUserName);
            if (!pUserName || wcslen(pUserName) == 0) {
                continue;
            }

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSWinStationName, &pWinStation, &bytes);
            ScopedWtsMemory<wchar_t> stationHolder(pWinStation);

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSClientName, &pClientName, &bytes);
            ScopedWtsMemory<wchar_t> clientHolder(pClientName);

            PWTSINFOW pInfo = nullptr;
            std::wstring logonTime = L"-";
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSSessionInfo, reinterpret_cast<PWSTR*>(&pInfo), &bytes)) {
                ScopedWtsMemory<WTSINFOW> infoHolder(pInfo);
                if (pInfo && pInfo->LogonTime.QuadPart > 0) {
                    FILETIME ft;
                    ft.dwLowDateTime = pInfo->LogonTime.LowPart;
                    ft.dwHighDateTime = pInfo->LogonTime.HighPart;
                    SYSTEMTIME stUTC, stLocal;
                    FileTimeToSystemTime(&ft, &stUTC);
                    SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
                    wchar_t tbuf[64];
                    swprintf_s(tbuf, L"%02d:%02d", stLocal.wHour, stLocal.wMinute);
                    logonTime = tbuf;
                }
            }

            PinkySessionSummary summary;
            summary.username = pUserName;
            summary.fullName = GetUserFullName(pUserName);
            summary.line = (pWinStation && wcslen(pWinStation) > 0) ? pWinStation : L"console";
            summary.idle = (sessionHolder.Get()[i].State == WTSActive) ? L"*" : L"idle";
            summary.logonTime = logonTime;
            summary.host = (pClientName && wcslen(pClientName) > 0) ? pClientName : L"local";

            list.push_back(summary);
        }

        return list;
    }

    static PinkyUserProfile QueryUserProfile(const std::wstring& username, bool printPlan) {
        PinkyUserProfile profile;
        profile.username = username;

        LPUSER_INFO_2 pBuf = nullptr;
        NET_API_STATUS status = NetUserGetInfo(nullptr, username.c_str(), 2, reinterpret_cast<LPBYTE*>(&pBuf));

        if (status != NERR_Success || !pBuf) {
            profile.userFound = false;
            return profile;
        }
        ScopedNetApiMemory<USER_INFO_2> bufHolder(pBuf);

        profile.userFound = true;
        profile.fullName = (pBuf->usri2_full_name && wcslen(pBuf->usri2_full_name) > 0) ? pBuf->usri2_full_name : username;
        profile.homeDir = (pBuf->usri2_home_dir && wcslen(pBuf->usri2_home_dir) > 0) ? pBuf->usri2_home_dir : (L"C:\\Users\\" + username);
        profile.comment = (pBuf->usri2_comment && wcslen(pBuf->usri2_comment) > 0) ? pBuf->usri2_comment : L"-";
        profile.shell = (pBuf->usri2_script_path && wcslen(pBuf->usri2_script_path) > 0) ? pBuf->usri2_script_path : L"C:\\Windows\\System32\\cmd.exe";
        profile.lastLogon = pBuf->usri2_last_logon;

        if (printPlan) {
            fs::path profilePath = fs::path(L"C:\\Users") / username / L".plan";
            if (!fs::exists(profilePath)) {
                profilePath = fs::path(L"C:\\Users") / username / L"plan.txt";
            }

            if (fs::exists(profilePath)) {
                std::wifstream planFile(profilePath);
                std::wstring line;
                while (std::getline(planFile, line)) {
                    profile.planLines.push_back(line);
                }
            }
        }

        return profile;
    }
};

// ============================================================================
// 4. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class PinkyOptions {
public:
    bool forceShort = false;
    bool forceLong = false;
    bool printPlan = true;
    std::vector<std::wstring> targets;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                for (int j = i + 1; j < argc; ++j) {
                    targets.push_back(argv[j]);
                }
                break;
            } else if (arg == L"-s" || arg == L"-q" || arg == L"--short") {
                forceShort = true;
            } else if (arg == L"-l" || arg == L"--long") {
                forceLong = true;
            } else if (arg == L"-p" || arg == L"--plan") {
                printPlan = false;
            } else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                showHelp = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            } else {
                targets.push_back(arg);
            }
        }
        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
        std::wcout << LR"(pinky(1)                CrossShell for UNIX Reference Manual                 pinky(1)

    NAME
        pinky - lightweight finger user information query tool

    SYNOPSIS
        pinky [OPTIONS] [USER...]

    DESCRIPTION
        pinky displays information about current Windows terminal, SSH, and local
        user sessions.

    OPTIONS
        -l
            Produce long format output for the specified USERs.

        -s, -q
            Produce short format output (default).

        -p
            Omit the user's plan file / notes in long format.

        -h
            Omit the project file / profile line in long format.

        --json, --csv, --table
            Output user records as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        pinky
            List all active interactive users in short format.

        pinky -l Administrator
            Display detailed long profile for Administrator.

    CrossShell for UNIX                                                  pinky(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"pinky 1.0.0\n";
    }
};

// ============================================================================
// 5. OUTPUT REPORTER
// ============================================================================

class PinkyReporter {
public:
    static void PrintSummary(const std::vector<PinkySessionSummary>& sessions) {
        std::wcout << std::left
                  << std::setw(14) << L"Login"
                  << std::setw(22) << L"Name"
                  << std::setw(12) << L"Tty"
                  << std::setw(8)  << L"Idle"
                  << std::setw(14) << L"Login Time"
                  << L"Office/Host\n";

        for (const auto& s : sessions) {
            std::wcout << std::left
                      << std::setw(14) << s.username
                      << std::setw(22) << s.fullName
                      << std::setw(12) << s.line
                      << std::setw(8)  << s.idle
                      << std::setw(14) << s.logonTime
                      << L"(" << s.host << L")\n";
        }
    }

    static void PrintProfile(const PinkyUserProfile& profile, bool printPlan) {
        if (!profile.userFound) {
            std::wcerr << L"pinky: " << profile.username << L": no such user.\n";
            return;
        }

        std::wcout << L"Login: " << std::left << std::setw(25) << profile.username 
                  << L"Name: " << profile.fullName << L"\n";
        std::wcout << L"Directory: " << std::left << std::setw(21) << profile.homeDir 
                  << L"Shell: " << profile.shell << L"\n";
        std::wcout << L"Comment: " << profile.comment << L"\n";

        if (profile.lastLogon > 0) {
            std::wcout << L"Last logon: " << PinkyProfileService::FormatTime(profile.lastLogon) << L"\n";
        } else {
            std::wcout << L"Never logged in.\n";
        }

        std::wcout << L"No Mail.\n";

        if (printPlan) {
            if (!profile.planLines.empty()) {
                std::wcout << L"Plan:\n";
                for (const auto& line : profile.planLines) {
                    std::wcout << line << L"\n";
                }
            } else {
                std::wcout << L"No Plan.\n";
            }
        }
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class PinkyApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        PinkyOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"pinky");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"pinky");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.targets.empty() || (opts.forceShort && opts.targets.empty())) {
            auto sessions = PinkyProfileService::QueryActiveSessions();
            PinkyReporter::PrintSummary(sessions);
            return 0;
        }

        for (const auto& target : opts.targets) {
            size_t atPos = target.find(L'@');
            if (atPos != std::wstring::npos) {
                std::wstring uW = target.substr(0, atPos);
                std::wstring hW = target.substr(atPos + 1);

                std::string userA = PinkyProfileService::WideToUtf8(uW);
                std::string hostA = PinkyProfileService::WideToUtf8(hW);

                std::wcout << L"[" << hW << L"]\n";
                FingerClient::QueryRemote(userA, hostA);
            } else {
                if (opts.forceShort) {
                    auto sessions = PinkyProfileService::QueryActiveSessions();
                    PinkyReporter::PrintSummary(sessions);
                } else {
                    PinkyUserProfile profile = PinkyProfileService::QueryUserProfile(target, opts.printPlan);
                    PinkyReporter::PrintProfile(profile, opts.printPlan);
                }
            }
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    PinkyApplication app;
    return app.Run(argc, argv);
}
