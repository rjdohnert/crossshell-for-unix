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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wtsapi32.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <sstream>
#include <clocale>
#include <cwctype>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "ws2_32.lib")

// Deleter for Win32 WTS allocated memory
struct WtsDeleter {
    void operator()(void* p) const {
        if (p) WTSFreeMemory(p);
    }
};

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    ULONGLONG cpuTime; // 100-ns units
};

struct SessionData {
    DWORD sessionId;
    std::wstring username;
    std::wstring tty;
    std::wstring fromHost;
    ULONGLONG logonTime;
    ULONGLONG idleTimeMs;
    ULONGLONG jcpu100ns;
    ULONGLONG pcpu100ns;
    std::wstring whatProcess;
    bool isConnected;
};

enum class OutputFormat { Human, Json, Csv, Tsv, Table };

class PipeBuffer : public std::streambuf {
    FILE* file_; char buffer_[4096];
public:
    explicit PipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }
    int sync() override { auto n = pptr() - pbase(); if (n) std::fwrite(pbase(), 1, static_cast<size_t>(n), file_); setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_); }
};

class PipeSession {
    std::streambuf* old_; FILE* file_ = nullptr; PipeBuffer* buffer_ = nullptr;
public:
    explicit PipeSession(const std::string& command) : old_(std::cout.rdbuf()) { if (!command.empty() && (file_ = _popen(command.c_str(), "w"))) { buffer_ = new PipeBuffer(file_); std::cout.rdbuf(buffer_); } }
    ~PipeSession() { std::cout.flush(); std::cout.rdbuf(old_); delete buffer_; if (file_) _pclose(file_); }
};

static std::string CsvQuote(const std::string& value) { std::string out = "\""; for (char ch : value) out += ch == '"' ? "\"\"" : std::string(1, ch); return out + "\""; }
static std::string JsonQuote(const std::string& value) { std::string out = "\""; for (char ch : value) { if (ch == '"' || ch == '\\') out += '\\'; if (ch == '\n') out += 'n'; else if (ch == '\r') out += 'r'; else out += ch; } return out + "\""; }

// Convert Wide String to UTF-8
static std::string WStrToStr(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &str[0], size, NULL, NULL);
    return str;
}

// Convert FILETIME to local time string ("10:15am" or "Oct14")
static std::string FormatLoginTime(ULONGLONG timestamp) {
    if (timestamp == 0) return " - ";
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(timestamp & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(timestamp >> 32);

    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    SYSTEMTIME stCurrent;
    GetLocalTime(&stCurrent);

    char buf[32];
    if (stLocal.wYear == stCurrent.wYear &&
        stLocal.wMonth == stCurrent.wMonth &&
        stLocal.wDay == stCurrent.wDay) {
        int hour = stLocal.wHour % 12;
        if (hour == 0) hour = 12;
        snprintf(buf, sizeof(buf), "%02d:%02d%s",
            hour, stLocal.wMinute, (stLocal.wHour >= 12) ? "pm" : "am");
    } else {
        static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        snprintf(buf, sizeof(buf), "%s%02d", months[(stLocal.wMonth - 1) % 12], stLocal.wDay);
    }
    return buf;
}

// Format Idle duration (e.g. "0s", "2m", "1:15", "2days")
static std::string FormatIdleTime(ULONGLONG idleMs) {
    ULONGLONG totalSec = idleMs / 1000;
    if (totalSec < 60) {
        return std::to_string(totalSec) + "s";
    }
    ULONGLONG totalMin = totalSec / 60;
    if (totalMin < 60) {
        return std::to_string(totalMin) + "m";
    }
    ULONGLONG hours = totalMin / 60;
    ULONGLONG mins = totalMin % 60;
    if (hours < 24) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%llu:%02llu", hours, mins);
        return buf;
    }
    ULONGLONG days = hours / 24;
    return std::to_string(days) + "days";
}

// Format CPU time (e.g. "0.12s", "1:05m")
static std::string FormatCpuTime(ULONGLONG cpu100ns) {
    double totalSec = (double)cpu100ns / 10000000.0;
    if (totalSec < 60.0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fs", totalSec);
        return buf;
    }
    ULONGLONG totalMin = (ULONGLONG)(totalSec / 60.0);
    double remSec = totalSec - (totalMin * 60.0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%llu:%02.0fm", totalMin, remSec);
    return buf;
}

// Calculate CPU Usage percentage
static double GetCpuUsagePercentage() {
    FILETIME ftIdle1, ftKernel1, ftUser1;
    FILETIME ftIdle2, ftKernel2, ftUser2;

    if (!GetSystemTimes(&ftIdle1, &ftKernel1, &ftUser1)) return 0.0;
    Sleep(100);
    if (!GetSystemTimes(&ftIdle2, &ftKernel2, &ftUser2)) return 0.0;

    ULONGLONG idle1 = ((ULONGLONG)ftIdle1.dwHighDateTime << 32) | ftIdle1.dwLowDateTime;
    ULONGLONG idle2 = ((ULONGLONG)ftIdle2.dwHighDateTime << 32) | ftIdle2.dwLowDateTime;

    ULONGLONG kernel1 = ((ULONGLONG)ftKernel1.dwHighDateTime << 32) | ftKernel1.dwLowDateTime;
    ULONGLONG kernel2 = ((ULONGLONG)ftKernel2.dwHighDateTime << 32) | ftKernel2.dwLowDateTime;

    ULONGLONG user1 = ((ULONGLONG)ftUser1.dwHighDateTime << 32) | ftUser1.dwLowDateTime;
    ULONGLONG user2 = ((ULONGLONG)ftUser2.dwHighDateTime << 32) | ftUser2.dwLowDateTime;

    ULONGLONG usrDiff = user2 - user1;
    ULONGLONG kerDiff = kernel2 - kernel1;
    ULONGLONG idlDiff = idle2 - idle1;

    ULONGLONG sysTotal = usrDiff + kerDiff;
    if (sysTotal == 0 || sysTotal <= idlDiff) return 0.0;

    ULONGLONG busyTotal = sysTotal - idlDiff;
    return (busyTotal * 100.0) / sysTotal;
}

// Print header info (Uptime, user count, CPU load)
static void PrintHeader(size_t activeUsers) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    int hour = st.wHour % 12;
    if (hour == 0) hour = 12;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d%s",
        hour, st.wMinute, (st.wHour >= 12) ? "pm" : "am");

    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG totalSec = uptimeMs / 1000;
    ULONGLONG days = totalSec / 86400;
    ULONGLONG hours = (totalSec % 86400) / 3600;
    ULONGLONG mins = (totalSec % 3600) / 60;

    double cpuUsage = GetCpuUsagePercentage();

    std::cout << "  " << timeBuf << "  up ";
    if (days > 0) {
        std::cout << days << " day(s), ";
    }
    std::cout << hours << ":" << std::setw(2) << std::setfill('0') << mins << std::setfill(' ') << ",  "
              << activeUsers << " user" << (activeUsers == 1 ? "" : "s") << ",  "
              << "cpu usage: " << std::fixed << std::setprecision(1) << cpuUsage << "%\n";
}
static void PrintHelp() {
    std::wcout << LR"(w(1)                    CrossShell for UNIX Reference Manual                    w(1)

    NAME
        w - show who is logged on and what they are doing

    SYNOPSIS
        w [OPTIONS] [USER]

    DESCRIPTION
        w displays information about the users currently logged on to the
        system and their active processes. The header displays the current
        local time, system uptime, total active logged-in user count, and
        CPU utilization. Each session record reports the user account name,
        terminal station (TTY), remote source address (FROM), login timestamp,
        idle duration, total session CPU time (JCPU), active process CPU time
        (PCPU), and the current foreground process (WHAT).

    OPTIONS
        -h, --no-header
            Suppress the system summary header line.

        -s, --short
            Use short output format; omit Login Time, JCPU, and PCPU columns.

        -f, --from
            Include the 'FROM' column displaying remote client IP or hostname.

        -l, --long
            Use long output format (default).

        -i, --ip-addr
            Display IP addresses instead of hostnames for remote sessions.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -?, -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    COLUMN DESCRIPTIONS
        User
            Logged-in Windows user account name.

        TTY
            Terminal Station name (Console, RDP-Tcp#0, etc.).

        FROM
            Remote IPv4/IPv6 client address or hostname.

        Login@
            Local timestamp when the user session logged in.

        Idle
            Duration since the user last interacted with the session.

        JCPU
            Total CPU time accumulated by all processes in the session.

        PCPU
            CPU time consumed by the active foreground process (WHAT).

        WHAT
            The active command or process running in the session.

    EXAMPLES
        w
            Display full active user session table and system summary.

        w -f
            Display session table with remote client IP addresses.

        w -s
            Display abbreviated summary format.

        w -h
            Display session table without the system summary header.

        w Administrator
            Display active session details for user 'Administrator' only.

        w --json
            Export active user session records formatted as JSON.

    CrossShell for UNIX                                                        w(1)
)";
}

static void PrintVersion() {
    std::wcout << L"w (CrossShell) 5.0.0\n"
               << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");

    bool showHeader = true;
    bool shortFormat = false;
    bool showFrom = false;
    std::wstring targetUser = L"";
    OutputFormat outputFormat = OutputFormat::Human;
    std::string pipeCommand;

    // CLI Arguments Parser
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-?" || arg == L"--help") {
            PrintHelp();
            return 0;
        } else if (arg == L"-V" || arg == L"--version") {
            PrintVersion();
            return 0;
        } else if (arg == L"-h" || arg == L"--no-header") {
            showHeader = false;
        } else if (arg == L"-s" || arg == L"--short") {
            shortFormat = true;
        } else if (arg == L"-f" || arg == L"--from" || arg == L"-i" || arg == L"--ip-addr") {
            showFrom = true;
        } else if (arg == L"-l" || arg == L"--long") {
            shortFormat = false;
        } else if (arg == L"--json" || arg == L"-j") {
            outputFormat = OutputFormat::Json;
        } else if (arg == L"--csv") {
            outputFormat = OutputFormat::Csv;
        } else if (arg == L"--tsv") {
            outputFormat = OutputFormat::Tsv;
        } else if (arg == L"--table") {
            outputFormat = OutputFormat::Table;
        } else if (arg == L"--output" && i + 1 < argc) {
            std::wstring fmt = argv[++i];
            if (fmt == L"json") outputFormat = OutputFormat::Json;
            else if (fmt == L"csv") outputFormat = OutputFormat::Csv;
            else if (fmt == L"tsv") outputFormat = OutputFormat::Tsv;
            else if (fmt == L"table") outputFormat = OutputFormat::Table;
        } else if (arg == L"--pipe" && i + 1 < argc) {
            std::wstring commandText = argv[++i];
            pipeCommand = WStrToStr(commandText);
        } else if (!arg.empty() && arg[0] != L'-') {
            targetUser = arg;
        }
    }

    PWTS_SESSION_INFOW pSessions = nullptr;
    DWORD sessionCount = 0;

    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessions, &sessionCount)) {
        std::cerr << "Error: Unable to enumerate terminal sessions (Error Code: " << GetLastError() << ")\n";
        return 1;
    }

    std::unique_ptr<WTS_SESSION_INFOW, WtsDeleter> spSessions(pSessions);
    PipeSession pipeSession(pipeCommand);

    // Enumerate Processes across all sessions for JCPU/PCPU/WHAT
    PWTS_PROCESS_INFOW pProcInfo = nullptr;
    DWORD procCount = 0;
    std::unordered_map<DWORD, std::vector<ProcessInfo>> sessionProcesses;

    if (WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pProcInfo, &procCount)) {
        std::unique_ptr<WTS_PROCESS_INFOW, WtsDeleter> spProc(pProcInfo);
        for (DWORD i = 0; i < procCount; ++i) {
            DWORD sId = pProcInfo[i].SessionId;
            DWORD pId = pProcInfo[i].ProcessId;
            std::wstring pName = L"unknown";
            ULONGLONG totalCpu = 0;

            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pId);
            if (!hProc) hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pId);

            if (hProc) {
                WCHAR imagePath[MAX_PATH] = {};
                DWORD pathLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, imagePath, &pathLen)) {
                    std::wstring fullPath(imagePath, pathLen);
                    size_t slash = fullPath.find_last_of(L"/\\");
                    pName = (slash == std::wstring::npos) ? fullPath : fullPath.substr(slash + 1);
                }

                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    ULONGLONG kTime = ((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime;
                    ULONGLONG uTime = ((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime;
                    totalCpu = kTime + uTime;
                }
                CloseHandle(hProc);

                sessionProcesses[sId].push_back({ pId, pName, totalCpu });
            }
        }
    }

    std::vector<SessionData> userSessions;

    // Collect session details
    for (DWORD i = 0; i < sessionCount; ++i) {
        DWORD sId = pSessions[i].SessionId;

        LPWSTR pUserBuf = nullptr;
        DWORD bytesRet = 0;

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSUserName, &pUserBuf, &bytesRet)) {
            std::wstring user = pUserBuf ? pUserBuf : L"";
            WTSFreeMemory(pUserBuf);

            if (user.empty()) continue; // Skip unauthenticated/system sessions

            if (!targetUser.empty() && _wcsicmp(user.c_str(), targetUser.c_str()) != 0) {
                continue; // User filter
            }

            SessionData data = {};
            data.sessionId = sId;
            data.username = user;
            data.isConnected = (pSessions[i].State == WTSActive);

            // WinStation / TTY Name
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSWinStationName, &pUserBuf, &bytesRet)) {
                data.tty = pUserBuf ? pUserBuf : L"console";
                WTSFreeMemory(pUserBuf);
            } else {
                data.tty = L"console";
            }

            // Client Address (FROM)
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSClientAddress, &pUserBuf, &bytesRet)) {
                PWTS_CLIENT_ADDRESS pAddr = (PWTS_CLIENT_ADDRESS)pUserBuf;
                if (pAddr && pAddr->AddressFamily == AF_INET) {
                    char ipStr[64];
                    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                        pAddr->Address[2], pAddr->Address[3], pAddr->Address[4], pAddr->Address[5]);
                    std::string s(ipStr);
                    data.fromHost = std::wstring(s.begin(), s.end());
                } else if (pAddr && pAddr->AddressFamily == AF_INET6) {
                    char ipStr[INET6_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET6, &pAddr->Address[2], ipStr, sizeof(ipStr));
                    std::string s(ipStr);
                    data.fromHost = std::wstring(s.begin(), s.end());
                } else {
                    data.fromHost = L"local";
                }
                WTSFreeMemory(pUserBuf);
            } else {
                data.fromHost = L"local";
            }

            // Session Timestamps (Logon, Idle)
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSSessionInfo, &pUserBuf, &bytesRet)) {
                PWTSINFOW pInfo = (PWTSINFOW)pUserBuf;
                data.logonTime = pInfo->LogonTime.QuadPart;

                FILETIME ftCurrent;
                GetSystemTimeAsFileTime(&ftCurrent);
                ULONGLONG nowFt = ((ULONGLONG)ftCurrent.dwHighDateTime << 32) | ftCurrent.dwLowDateTime;
                ULONGLONG lastInFt = pInfo->LastInputTime.QuadPart;

                if (nowFt > lastInFt && lastInFt > 0) {
                    data.idleTimeMs = (nowFt - lastInFt) / 10000ULL;
                } else {
                    data.idleTimeMs = 0;
                }
                WTSFreeMemory(pUserBuf);
            }

            // Process CPU aggregation (JCPU, PCPU, WHAT)
            ULONGLONG totalJcpu = 0;
            ULONGLONG maxPcpu = 0;
            std::wstring topProc = L"cmd.exe";

            auto pIt = sessionProcesses.find(sId);
            if (pIt != sessionProcesses.end()) {
                for (const auto& proc : pIt->second) {
                    totalJcpu += proc.cpuTime;

                    // Exclude system background processes for 'WHAT'
                    std::wstring lowerName = proc.name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](wchar_t ch) {
                        return std::towlower(ch);
                    });

                    if (lowerName != L"explorer.exe" && lowerName != L"svchost.exe" && lowerName != L"csrss.exe") {
                        if (proc.cpuTime >= maxPcpu) {
                            maxPcpu = proc.cpuTime;
                            topProc = proc.name;
                        }
                    }
                }
            }

            data.jcpu100ns = totalJcpu;
            data.pcpu100ns = maxPcpu;
            data.whatProcess = topProc;

            userSessions.push_back(data);
        }
    }

    if (showHeader && outputFormat == OutputFormat::Human) {
        PrintHeader(userSessions.size());
    }

    if (userSessions.empty()) {
        return 0;
    }

    if (outputFormat == OutputFormat::Csv) std::cout << "user,tty,from,login,idle,jcpu,pcpu,what\n";
    if (outputFormat == OutputFormat::Tsv) std::cout << "USER\tTTY\tFROM\tLOGIN\tIDLE\tJCPU\tPCPU\tWHAT\n";
    if (outputFormat == OutputFormat::Table) std::cout << "USER\tTTY\tFROM\tLOGIN\tIDLE\tJCPU\tPCPU\tWHAT\n";

    // Print Column Headers
    if (outputFormat != OutputFormat::Human) {
        for (const auto& s : userSessions) {
            std::string u = WStrToStr(s.username), t = WStrToStr(s.tty), f = WStrToStr(s.fromHost), w = WStrToStr(s.whatProcess);
            std::string l = FormatLoginTime(s.logonTime), idle = FormatIdleTime(s.idleTimeMs), j = FormatCpuTime(s.jcpu100ns), p = FormatCpuTime(s.pcpu100ns);
            if (outputFormat == OutputFormat::Json) {
                std::cout << "{\"user\":" << JsonQuote(u) << ",\"tty\":" << JsonQuote(t) << ",\"from\":" << JsonQuote(f) << ",\"login\":" << JsonQuote(l) << ",\"idle\":" << JsonQuote(idle) << ",\"jcpu\":" << JsonQuote(j) << ",\"pcpu\":" << JsonQuote(p) << ",\"what\":" << JsonQuote(w) << "}\n";
            } else if (outputFormat == OutputFormat::Csv) {
                std::cout << CsvQuote(u) << ',' << CsvQuote(t) << ',' << CsvQuote(f) << ',' << CsvQuote(l) << ',' << CsvQuote(idle) << ',' << CsvQuote(j) << ',' << CsvQuote(p) << ',' << CsvQuote(w) << "\n";
            } else {
                std::cout << u << '\t' << t << '\t' << f << '\t' << l << '\t' << idle << '\t' << j << '\t' << p << '\t' << w << "\n";
            }
        }
        return 0;
    }

    std::cout << std::left << std::setw(12) << "User"
              << std::setw(12) << "TTY";

    if (showFrom) {
        std::cout << std::setw(18) << "FROM";
    }

    if (!shortFormat) {
        std::cout << std::setw(10) << "Login@"
                  << std::setw(8)  << "Idle"
                  << std::setw(9)  << "JCPU"
                  << std::setw(9)  << "PCPU";
    } else {
        std::cout << std::setw(8) << "Idle";
    }

    std::cout << "WHAT\n";

    // Print User Session Rows
    for (const auto& s : userSessions) {
        std::string uStr = WStrToStr(s.username);
        std::string tStr = WStrToStr(s.tty);
        std::string fStr = WStrToStr(s.fromHost);
        std::string lStr = FormatLoginTime(s.logonTime);
        std::string iStr = FormatIdleTime(s.idleTimeMs);
        std::string jStr = FormatCpuTime(s.jcpu100ns);
        std::string pStr = FormatCpuTime(s.pcpu100ns);
        std::string wStr = WStrToStr(s.whatProcess);

        std::cout << std::left << std::setw(12) << uStr
                  << std::setw(12) << tStr;

        if (showFrom) {
            std::cout << std::setw(18) << fStr;
        }

        if (!shortFormat) {
            std::cout << std::setw(10) << lStr
                      << std::setw(8)  << iStr
                      << std::setw(9)  << jStr
                      << std::setw(9)  << pStr;
        } else {
            std::cout << std::setw(8) << iStr;
        }

        std::cout << wStr << "\n";
    }

    return 0;
}