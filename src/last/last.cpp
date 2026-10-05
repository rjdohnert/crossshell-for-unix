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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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

#include <windows.h>
#include <winevt.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <clocale>

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "advapi32.lib")

// Check if process has Administrator rights required for Windows Security log
bool IsUserAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

// RAII Wrapper for EVT_HANDLE
class ScopedEvtHandle {
    EVT_HANDLE m_h;
public:
    ScopedEvtHandle(EVT_HANDLE h = NULL) : m_h(h) {}
    ~ScopedEvtHandle() { if (m_h) EvtClose(m_h); }
    EVT_HANDLE get() const { return m_h; }
    EVT_HANDLE* operator&() { return &m_h; }
    operator EVT_HANDLE() const { return m_h; }
    ScopedEvtHandle& operator=(EVT_HANDLE h) {
        if (m_h) EvtClose(m_h);
        m_h = h;
        return *this;
    }
};

struct EventRecord {
    DWORD eventId;
    ULONGLONG timestamp;
    std::wstring username;
    std::wstring logonId;
    DWORD logonType;
    std::wstring ipOrHost;
};

struct DisplayRecord {
    std::wstring username;
    std::wstring tty;
    std::wstring host;
    ULONGLONG loginTime;
    ULONGLONG logoutTime;
    bool stillLoggedIn;
    bool stillRunning;
    bool wasShutdownTerminated = false;
};

// Convert Wide String to UTF-8
std::string WStrToStr(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &str[0], size, NULL, NULL);
    return str;
}

// Safe XML Tag & Data Extractors
std::wstring ExtractXmlTag(const std::wstring& xml, const std::wstring& tag) {
    std::wstring openTag = L"<" + tag + L">";
    size_t start = xml.find(openTag);
    if (start == std::wstring::npos) return L"";
    start += openTag.length();
    size_t end = xml.find(L"</" + tag + L">", start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

std::wstring ExtractXmlAttr(const std::wstring& xml, const std::wstring& tag, const std::wstring& attr) {
    size_t tagPos = xml.find(L"<" + tag);
    if (tagPos == std::wstring::npos) return L"";

    std::wstring p1 = attr + L"='";
    std::wstring p2 = attr + L"=\"";

    size_t attrPos = xml.find(p1, tagPos);
    wchar_t quote = L'\'';
    size_t len = p1.length();

    if (attrPos == std::wstring::npos) {
        attrPos = xml.find(p2, tagPos);
        quote = L'"';
        len = p2.length();
    }
    if (attrPos == std::wstring::npos) return L"";

    size_t start = attrPos + len;
    size_t end = xml.find(quote, start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

std::wstring ExtractXmlData(const std::wstring& xml, const std::wstring& dataName) {
    std::wstring p1 = L"Name='" + dataName + L"'>";
    std::wstring p2 = L"Name=\"" + dataName + L"\">";

    size_t pos = xml.find(p1);
    size_t len = p1.length();
    if (pos == std::wstring::npos) {
        pos = xml.find(p2);
        len = p2.length();
    }
    if (pos == std::wstring::npos) return L"";

    size_t start = pos + len;
    size_t end = xml.find(L"</Data>", start);
    if (end == std::wstring::npos) return L"";
    return xml.substr(start, end - start);
}

// Convert ISO8601 XML SystemTime string to Timestamp
ULONGLONG ISO8601ToTimestamp(const std::wstring& isoStr) {
    if (isoStr.empty()) return 0;
    SYSTEMTIME st = { 0 };
    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;
    if (swscanf_s(isoStr.c_str(), L"%d-%d-%dT%d:%d:%d",
        &year, &month, &day, &hour, &min, &sec) >= 6) {
        st.wYear = (WORD)year;
        st.wMonth = (WORD)month;
        st.wDay = (WORD)day;
        st.wHour = (WORD)hour;
        st.wMinute = (WORD)min;
        st.wSecond = (WORD)sec;

        FILETIME ftUtc;
        if (SystemTimeToFileTime(&st, &ftUtc)) {
            return ((ULONGLONG)ftUtc.dwHighDateTime << 32) | ftUtc.dwLowDateTime;
        }
    }
    return 0;
}

std::string FormatTime(ULONGLONG timestamp) {
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(timestamp & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(timestamp >> 32);

    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s %2d %02d:%02d",
        days[stLocal.wDayOfWeek % 7],
        months[(stLocal.wMonth - 1) % 12],
        stLocal.wDay,
        stLocal.wHour,
        stLocal.wMinute);
    return buf;
}

std::string FormatDuration(ULONGLONG startFt, ULONGLONG endFt) {
    if (endFt <= startFt) return "(00:00)";
    ULONGLONG diffSec = (endFt - startFt) / 10000000ULL;

    ULONGLONG days = diffSec / 86400;
    ULONGLONG hours = (diffSec % 86400) / 3600;
    ULONGLONG minutes = (diffSec % 3600) / 60;

    char buf[32];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "(%llud+%02llu:%02llu)", days, hours, minutes);
    } else {
        snprintf(buf, sizeof(buf), "(%02llu:%02llu)", hours, minutes);
    }
    return buf;
}

std::wstring LogonTypeToTTY(DWORD type) {
    switch (type) {
    case 2:  return L"console";
    case 3:  return L"network";
    case 7:  return L"unlock";
    case 8:  return L"netclear";
    case 10: return L"pts/rdp";
    case 11: return L"cached";
    default: return L"session";
    }
}

bool IsSystemNoiseUser(const std::wstring& user) {
    if (user.empty()) return true;
    if (user.back() == L'$') return true; // Computer accounts

    static const std::unordered_set<std::wstring> noise = {
        L"SYSTEM", L"LOCAL SERVICE", L"NETWORK SERVICE", L"ANONYMOUS LOGON"
    };
    if (noise.count(user)) return true;
    if (user.rfind(L"DWM-", 0) == 0 || user.rfind(L"UMFD-", 0) == 0) return true;

    return false;
}

void QueryChannel(LPCWSTR channelOrFile, LPCWSTR xpathQuery, bool isFile, std::vector<EventRecord>& records) {
    DWORD flags = isFile ? EvtQueryFilePath : EvtQueryChannelPath;
    flags |= EvtQueryReverseDirection;

    ScopedEvtHandle hQuery(EvtQuery(NULL, channelOrFile, xpathQuery, flags));
    if (!hQuery.get()) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            std::cerr << "[ERROR] Access Denied opening log: " << WStrToStr(channelOrFile)
                      << "\nPlease run this terminal session as Administrator.\n";
        }
        return;
    }

    EVT_HANDLE hEvents[100];
    DWORD dwReturned = 0;
    DWORD dwBufferSize = 16384;
    std::vector<wchar_t> buffer(dwBufferSize);

    while (EvtNext(hQuery, 100, hEvents, INFINITE, 0, &dwReturned)) {
        for (DWORD i = 0; i < dwReturned; i++) {
            ScopedEvtHandle hEvent(hEvents[i]);
            DWORD dwBufferUsed = 0;
            DWORD dwPropertyCount = 0;

            // Render Event XML cleanly (never fails on missing optional properties)
            if (!EvtRender(NULL, hEvent, EvtRenderEventXml, static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    buffer.resize(dwBufferUsed / sizeof(wchar_t) + 1);
                    if (!EvtRender(NULL, hEvent, EvtRenderEventXml, static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
                        continue;
                    }
                } else {
                    continue;
                }
            }

            std::wstring xml(buffer.data());

            EventRecord rec = {};
            std::wstring idStr = ExtractXmlTag(xml, L"EventID");
            rec.eventId = idStr.empty() ? 0 : _wtoi(idStr.c_str());

            std::wstring timeStr = ExtractXmlAttr(xml, L"TimeCreated", L"SystemTime");
            rec.timestamp = ISO8601ToTimestamp(timeStr);

            rec.username = ExtractXmlData(xml, L"TargetUserName");
            rec.logonId = ExtractXmlData(xml, L"TargetLogonId");
            std::transform(rec.logonId.begin(), rec.logonId.end(), rec.logonId.begin(), ::towlower);

            std::wstring lTypeStr = ExtractXmlData(xml, L"LogonType");
            rec.logonType = lTypeStr.empty() ? 0 : _wtoi(lTypeStr.c_str());

            std::wstring ipStr = ExtractXmlData(xml, L"IpAddress");
            std::wstring wsStr = ExtractXmlData(xml, L"WorkstationName");

            if (!ipStr.empty() && ipStr != L"-" && ipStr != L"::1") {
                rec.ipOrHost = ipStr;
            } else if (!wsStr.empty() && wsStr != L"-") {
                rec.ipOrHost = wsStr;
            } else {
                rec.ipOrHost = L"127.0.0.1";
            }

            records.push_back(rec);
        }
    }
}

void PrintUsage(const wchar_t* progName) {
    std::wcout << L"Usage: " << progName << L" [-n number] [-f file] [user ...]\n"
               << L"Options:\n"
               << L"  -n <number>   Limit output to the specified number of lines.\n"
               << L"  -f <file>     Read from an exported .evtx log file instead of system event log.\n"
               << L"  -h, --help    Show this help message.\n";
}

static int last_main(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");

    int maxLines = -1;
    std::wstring evtxFile = L"";
    std::unordered_set<std::wstring> filterUsers;

    // CLI Arguments Parser
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == L"-n" && i + 1 < argc) {
            maxLines = _wtoi(argv[++i]);
        } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && iswdigit(arg[1])) {
            maxLines = _wtoi(arg.c_str() + 1);
        } else if (arg == L"-f" && i + 1 < argc) {
            evtxFile = argv[++i];
        } else if (arg[0] != L'-') {
            filterUsers.insert(argv[i]);
        }
    }

    if (evtxFile.empty() && !IsUserAdmin()) {
        std::cerr << "[!] WARNING: Administrator access required.\n"
                  << "    Please run in an Administrator terminal session.\n\n";
    }

    std::vector<EventRecord> events;

    if (!evtxFile.empty()) {
        LPCWSTR fileQuery = L"*[System[(EventID=4624 or EventID=4634 or EventID=4647 or EventID=6005 or EventID=6006 or EventID=1074)]]";
        QueryChannel(evtxFile.c_str(), fileQuery, true, events);
    } else {
        // Query live Security (Logons) and System (Reboots)
        LPCWSTR secQuery = L"*[System[(EventID=4624 or EventID=4634 or EventID=4647)]]";
        LPCWSTR sysQuery = L"*[System[(EventID=6005 or EventID=6006 or EventID=1074)]]";
        QueryChannel(L"Security", secQuery, false, events);
        QueryChannel(L"System", sysQuery, false, events);
    }

    if (events.empty()) {
        std::cout << "No matching login or system events found.\n";
        return 0;
    }

    // Sort chronologically ascending for session duration logic
    std::sort(events.begin(), events.end(), [](const EventRecord& a, const EventRecord& b) {
        return a.timestamp < b.timestamp;
    });

    ULONGLONG earliestTimestamp = events.front().timestamp;

    std::unordered_map<std::wstring, ULONGLONG> activeLogoffs;
    ULONGLONG lastShutdownTime = 0;
    std::vector<DisplayRecord> displayList;

    // Process from newest to oldest
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        const auto& rec = *it;

        if (rec.eventId == 6006 || rec.eventId == 1074) { // System Shutdown
            lastShutdownTime = rec.timestamp;
        } 
        else if (rec.eventId == 6005) { // System Boot
            DisplayRecord disp;
            disp.username = L"reboot";
            disp.tty = L"system boot";
            disp.host = L"";
            disp.loginTime = rec.timestamp;
            disp.logoutTime = lastShutdownTime;
            disp.stillLoggedIn = false;
            disp.stillRunning = (lastShutdownTime == 0);

            displayList.push_back(disp);
            lastShutdownTime = 0;
        } 
        else if (rec.eventId == 4634 || rec.eventId == 4647) { // Logoff
            if (!rec.logonId.empty()) {
                activeLogoffs[rec.logonId] = rec.timestamp;
            }
        } 
        else if (rec.eventId == 4624) { // Logon
            if (!filterUsers.empty()) {
                bool matched = false;
                for (const auto& u : filterUsers) {
                    if (_wcsicmp(rec.username.c_str(), u.c_str()) == 0) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) continue;
            } else if (IsSystemNoiseUser(rec.username)) {
                continue; // Skip system accounts
            }

            DisplayRecord disp;
            disp.username = rec.username;
            disp.tty = LogonTypeToTTY(rec.logonType);
            disp.host = rec.ipOrHost;
            disp.loginTime = rec.timestamp;
            disp.stillRunning = false;
            disp.wasShutdownTerminated = false;

            auto lIt = activeLogoffs.find(rec.logonId);
            if (lIt != activeLogoffs.end()) {
                disp.logoutTime = lIt->second;
                disp.stillLoggedIn = false;
                activeLogoffs.erase(lIt);
            } else {
                disp.logoutTime = 0;
                disp.stillLoggedIn = true;
            }

            // Terminate session with shutdown time if it crossed a reboot/shutdown
            if (disp.stillLoggedIn && lastShutdownTime != 0 && disp.loginTime < lastShutdownTime) {
                disp.stillLoggedIn = false;
                disp.logoutTime = lastShutdownTime;
                disp.wasShutdownTerminated = true;
            }

            displayList.push_back(disp);
        }
    }

    if (maxLines > 0 && (size_t)maxLines < displayList.size()) {
        displayList.resize(maxLines);
    }

    for (const auto& disp : displayList) {
        std::string uStr = WStrToStr(disp.username);
        std::string tStr = WStrToStr(disp.tty);
        std::string hStr = WStrToStr(disp.host);
        std::string loginStr = FormatTime(disp.loginTime);

        std::cout << std::left 
                  << std::setw(16) << uStr 
                  << std::setw(12) << tStr 
                  << std::setw(16) << hStr 
                  << loginStr << " - ";

        if (disp.stillLoggedIn) {
            std::cout << "still logged in\n";
        } else if (disp.stillRunning) {
            std::cout << "still running\n";
        } else {
            FILETIME ftEnd;
            ftEnd.dwLowDateTime = (DWORD)(disp.logoutTime & 0xFFFFFFFF);
            ftEnd.dwHighDateTime = (DWORD)(disp.logoutTime >> 32);

            SYSTEMTIME stUTC, stLocal;
            FileTimeToSystemTime(&ftEnd, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", stLocal.wHour, stLocal.wMinute);

            std::string durStr = FormatDuration(disp.loginTime, disp.logoutTime);
            if (disp.wasShutdownTerminated) {
                std::cout << "down" << "  " << durStr << "\n";
            } else {
                std::cout << timeBuf << "  " << durStr << "\n";
            }
        }
    }

    std::cout << "\nwtmp begins " << FormatTime(earliestTimestamp) << "\n";
    return 0;
}

class LastApplication { public: int run(int argc, wchar_t* argv[]) const { return last_main(argc, argv); } };
int wmain(int argc, wchar_t* argv[]) { return LastApplication().run(argc, argv); }
