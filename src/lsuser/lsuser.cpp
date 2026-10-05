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
#include <lm.h>
#include <ntsecapi.h>

#include <iostream>
#include <string>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <sstream>
#include <cwctype>
#include <cwchar>
#include <memory>

#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Secur32.lib")

// ============================================================================
// 1. DATA MODELS & RAII HELPERS
// ============================================================================

template <typename T>
class ScopedNetApiBuffer {
public:
    explicit ScopedNetApiBuffer(T* ptr = nullptr) : m_ptr(ptr) {}
    ~ScopedNetApiBuffer() { Free(); }

    ScopedNetApiBuffer(const ScopedNetApiBuffer&) = delete;
    ScopedNetApiBuffer& operator=(const ScopedNetApiBuffer&) = delete;

    ScopedNetApiBuffer(ScopedNetApiBuffer&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedNetApiBuffer& operator=(ScopedNetApiBuffer&& other) noexcept {
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

struct AccountRecord {
    std::wstring name;
    std::wstring fullName;
    std::wstring homeDir;
    std::wstring shell;
    std::wstring comment;
    std::wstring groups;
    std::wstring loginDuration;
    std::wstring status;
    unsigned long uid = 0;
    bool disabled = false;
};

// ============================================================================
// 2. USER ACCOUNT ENUMERATOR & SERVICE
// ============================================================================

class UserAccountEnumerator {
public:
    static std::wstring ToWide(const std::string& input) {
        if (input.empty()) return L"";
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
        if (sizeNeeded <= 1) return L"";
        std::wstring result(static_cast<size_t>(sizeNeeded - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, &result[0], sizeNeeded);
        return result;
    }

    static std::wstring ToLower(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            return std::towlower(ch);
        });
        return value;
    }

    static std::wstring NormalizeText(const wchar_t* value) {
        if (value == nullptr || *value == L'\0') {
            return L"(none)";
        }
        return value;
    }

    static std::wstring NormalizeText(const std::wstring& value) {
        if (value.empty()) {
            return L"(none)";
        }
        return value;
    }

    static std::wstring ResolveFullName(const std::wstring& username, bool disabled) {
        if (disabled) return L"(none)";
        LPUSER_INFO_3 userInfo = nullptr;
        NET_API_STATUS status = NetUserGetInfo(nullptr, const_cast<LPWSTR>(username.c_str()), 3, reinterpret_cast<LPBYTE*>(&userInfo));
        if (status == NERR_Success && userInfo != nullptr) {
            ScopedNetApiBuffer<USER_INFO_3> buf(userInfo);
            std::wstring fullName = NormalizeText(userInfo->usri3_full_name);
            if (fullName != L"(none)") return fullName;
        }
        return L"(none)";
    }

    static std::wstring ResolveHomeDirectory(const std::wstring& username, bool disabled) {
        if (disabled) return L"(none)";
        LPUSER_INFO_3 userInfo = nullptr;
        NET_API_STATUS status = NetUserGetInfo(nullptr, const_cast<LPWSTR>(username.c_str()), 3, reinterpret_cast<LPBYTE*>(&userInfo));
        if (status == NERR_Success && userInfo != nullptr) {
            ScopedNetApiBuffer<USER_INFO_3> buf(userInfo);
            std::wstring home = NormalizeText(userInfo->usri3_home_dir);
            if (home != L"(none)") return home;
        }
        return L"(none)";
    }

    static std::wstring ResolveDefaultShell() {
        wchar_t comspec[MAX_PATH] = { 0 };
        DWORD size = GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH);
        if (size == 0 || comspec[0] == L'\0') {
            return L"cmd.exe";
        }

        std::wstring shell = comspec;
        std::wstring lower = ToLower(shell);
        if (lower.find(L"powershell") != std::wstring::npos || lower.find(L"pwsh") != std::wstring::npos) {
            return L"powershell.exe";
        }
        if (lower.find(L"cmd") != std::wstring::npos) {
            return L"cmd.exe";
        }
        return shell;
    }

    static std::wstring FormatDurationFromSeconds(ULONGLONG seconds) {
        if (seconds == 0) return L"0s";

        ULONGLONG days = seconds / 86400ULL;
        seconds %= 86400ULL;
        ULONGLONG hours = seconds / 3600ULL;
        seconds %= 3600ULL;
        ULONGLONG minutes = seconds / 60ULL;
        seconds %= 60ULL;

        std::wstringstream stream;
        if (days > 0) stream << days << L"d ";
        if (hours > 0 || days > 0) stream << hours << L"h ";
        if (minutes > 0 || hours > 0 || days > 0) stream << minutes << L"m ";
        stream << seconds << L"s";
        return stream.str();
    }

    static std::wstring ResolveGroupMembership(const std::wstring& username) {
        LPLOCALGROUP_USERS_INFO_0 groups = nullptr;
        DWORD entriesRead = 0;
        DWORD totalEntries = 0;

        NET_API_STATUS status = NetUserGetLocalGroups(
            nullptr,
            const_cast<LPWSTR>(username.c_str()),
            0,
            0,
            reinterpret_cast<LPBYTE*>(&groups),
            MAX_PREFERRED_LENGTH,
            &entriesRead,
            &totalEntries);

        if (status != NERR_Success && status != ERROR_MORE_DATA) {
            return L"(none)";
        }
        ScopedNetApiBuffer<LOCALGROUP_USERS_INFO_0> buf(groups);

        std::vector<std::wstring> names;
        for (DWORD i = 0; i < entriesRead; ++i) {
            if (groups[i].lgrui0_name != nullptr) {
                names.push_back(NormalizeText(groups[i].lgrui0_name));
            }
        }

        if (names.empty()) return L"(none)";

        std::wstringstream stream;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) stream << L", ";
            stream << names[i];
        }
        return stream.str();
    }

    static std::wstring ResolveLoginDuration(const std::wstring& username) {
        DWORD sessionCount = 0;
        PLUID sessionList = nullptr;
        NTSTATUS status = LsaEnumerateLogonSessions(&sessionCount, &sessionList);
        if (status != 0 || sessionList == nullptr) {
            return L"(none)";
        }

        std::wstring result = L"(none)";
        for (DWORD i = 0; i < sessionCount; ++i) {
            PSECURITY_LOGON_SESSION_DATA sessionData = nullptr;
            NTSTATUS dataStatus = LsaGetLogonSessionData(&sessionList[i], &sessionData);
            if (dataStatus == 0 && sessionData != nullptr) {
                std::wstring sessionUser(sessionData->UserName.Buffer, sessionData->UserName.Length / sizeof(wchar_t));
                if (ToLower(sessionUser) == ToLower(username)) {
                    ULARGE_INTEGER logonTime;
                    logonTime.QuadPart = sessionData->LogonTime.QuadPart;

                    FILETIME nowFileTime;
                    GetSystemTimeAsFileTime(&nowFileTime);
                    ULARGE_INTEGER now;
                    now.LowPart = nowFileTime.dwLowDateTime;
                    now.HighPart = nowFileTime.dwHighDateTime;

                    ULONGLONG elapsed100ns = now.QuadPart > logonTime.QuadPart ? now.QuadPart - logonTime.QuadPart : 0;
                    ULONGLONG seconds = elapsed100ns / 10000000ULL;
                    result = (seconds == 0) ? L"0s" : FormatDurationFromSeconds(seconds);
                }
                LsaFreeReturnBuffer(sessionData);
            }
        }

        LsaFreeReturnBuffer(sessionList);
        return result;
    }

    static std::vector<AccountRecord> EnumerateLocalAccounts() {
        std::vector<AccountRecord> accounts;
        LPUSER_INFO_1 users = nullptr;
        DWORD entriesRead = 0;
        DWORD totalEntries = 0;
        DWORD resumeHandle = 0;

        NET_API_STATUS status = NetUserEnum(
            nullptr,
            1,
            FILTER_NORMAL_ACCOUNT,
            reinterpret_cast<LPBYTE*>(&users),
            MAX_PREFERRED_LENGTH,
            &entriesRead,
            &totalEntries,
            &resumeHandle);

        if (status != NERR_Success && status != ERROR_MORE_DATA) {
            return accounts;
        }
        ScopedNetApiBuffer<USER_INFO_1> buf(users);

        std::wstring defaultShell = ResolveDefaultShell();

        for (DWORD i = 0; i < entriesRead; ++i) {
            AccountRecord account;
            account.name = NormalizeText(users[i].usri1_name);
            account.fullName = ResolveFullName(account.name, account.disabled);
            account.disabled = (users[i].usri1_flags & UF_ACCOUNTDISABLE) != 0;
            account.homeDir = ResolveHomeDirectory(account.name, account.disabled);
            account.shell = defaultShell;
            account.comment = NormalizeText(users[i].usri1_comment);
            account.groups = ResolveGroupMembership(account.name);
            account.loginDuration = ResolveLoginDuration(account.name);
            account.uid = 1000 + i + 1;
            account.status = account.disabled ? L"disabled" : L"active";
            accounts.push_back(account);
        }

        return accounts;
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

enum class LsuserFormat { Table, Csv, Json };

class LsuserOptions {
public:
    std::vector<std::string> requestedAttributes;
    std::vector<std::wstring> requestedUsers;
    LsuserFormat format = LsuserFormat::Table;
    bool showHelp = false;
    bool showVersion = false;

    static std::vector<std::string> SplitAttributes(const std::string& value) {
        std::vector<std::string> attrs;
        std::stringstream ss(value);
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) attrs.push_back(item);
        }
        return attrs;
    }

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    requestedUsers.push_back(UserAccountEnumerator::ToWide(argv[i]));
                }
                break;
            } else if (arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "--table") {
                format = LsuserFormat::Table;
            } else if (arg == "--csv") {
                format = LsuserFormat::Csv;
            } else if (arg == "--json") {
                format = LsuserFormat::Json;
            } else if (arg == "-") {
                std::string user;
                while (std::cin >> user) requestedUsers.push_back(UserAccountEnumerator::ToWide(user));
            } else if (arg == "-a" || arg == "--attributes") {
                if (i + 1 >= argc) {
                    std::cerr << "lsuser: option requires an argument -- 'a'\n";
                    return false;
                }
                requestedAttributes = SplitAttributes(argv[++i]);
            } else if (arg.rfind("-", 0) == 0) {
                std::cerr << "lsuser: invalid option -- '" << arg << "'\n";
                return false;
            } else {
                requestedUsers.push_back(UserAccountEnumerator::ToWide(arg));
            }
        }

        if (requestedAttributes.empty()) {
            requestedAttributes = { "Account", "UID", "Groups", "Login", "Status" };
        }

        return true;
    }

    void PrintUsage(const char* /*prog*/ = "lsuser") const {
        std::cout << R"(lsuser(1)                CrossShell for UNIX Reference Manual                 lsuser(1)

    NAME
        lsuser - list local Windows user accounts

    SYNOPSIS
        lsuser [OPTIONS] [USER]...

    DESCRIPTION
        Lists local user accounts in an AIX-like format. Users may be selected
        as positional arguments or read from standard input.

    OPTIONS
        -a, --attributes <attrs>
            Select comma-separated attributes: account, uid, fullname, home,
            groups, login, and status.

        --table
            Output an aligned table (default).

        --csv
            Output CSV.

        --json
            Output JSON.

        -
            Read user names from standard input.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        lsuser
            List all local user accounts.

        lsuser -a account,uid,status
            Display only account name, UID, and status columns.

        lsuser Administrator
            Display account information for Administrator only.

    EXIT STATUS
        0
            Success or no matching users.
        1
            Parse, option, enumeration, or missing-argument failure.

    CrossShell for UNIX                                                    lsuser(1)
)";
    }

    void PrintVersion() const {
        std::cout << "lsuser 1.0.0\n";
    }
};

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class LsuserReporter {
public:
    static std::wstring GetDisplayValue(const AccountRecord& account, const std::string& attribute) {
        std::string attr = attribute;
        std::transform(attr.begin(), attr.end(), attr.begin(), [](char ch) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        });

        if (attr == "account" || attr == "name") return account.name;
        if (attr == "uid") return std::to_wstring(account.uid);
        if (attr == "fullname" || attr == "full_name" || attr == "full-name") return account.fullName;
        if (attr == "home" || attr == "home_dir" || attr == "home-dir" || attr == "homedir") return account.homeDir;
        if (attr == "shell") return account.shell.empty() ? L"cmd.exe" : account.shell;
        if (attr == "comment") return account.comment;
        if (attr == "groups" || attr == "group" || attr == "group_membership") return account.groups;
        if (attr == "login" || attr == "logon" || attr == "login_time" || attr == "logon_time" || attr == "duration") return account.loginDuration;
        if (attr == "status") return account.status;
        return L"";
    }

    static int GetColumnWidth(const std::string& attribute) {
        std::string attr = attribute;
        std::transform(attr.begin(), attr.end(), attr.begin(), [](char ch) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        });

        if (attr == "shell") return 24;
        if (attr == "groups" || attr == "group" || attr == "group_membership") return 32;
        if (attr == "login" || attr == "logon" || attr == "login_time" || attr == "logon_time" || attr == "duration") return 18;
        if (attr == "fullname" || attr == "full_name" || attr == "full-name" || attr == "comment") return 28;
        if (attr == "home" || attr == "home_dir" || attr == "home-dir" || attr == "homedir") return 24;
        if (attr == "account" || attr == "name") return 20;
        if (attr == "uid") return 8;
        if (attr == "status") return 12;
        return 20;
    }

    static std::string ToUtf8(const std::wstring& value) {
        int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(n), '\0');
        if (n > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
        return out;
    }

    static std::string CsvEscape(const std::string& value) {
        std::string out = "\"";
        for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
        return out + '"';
    }

    static std::string JsonEscape(const std::string& value) {
        std::string out;
        for (char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c;
        }
        return out;
    }

    static void Emit(const LsuserOptions& opts, const std::vector<AccountRecord>& accounts) {
        if (opts.format == LsuserFormat::Csv) {
            std::cout << "Account,UID,Groups,Login,Status\n";
            for (const auto& a : accounts) {
                std::cout << CsvEscape(ToUtf8(a.name)) << ','
                          << a.uid << ','
                          << CsvEscape(ToUtf8(a.groups)) << ','
                          << CsvEscape(ToUtf8(a.loginDuration)) << ','
                          << CsvEscape(ToUtf8(a.status)) << '\n';
            }
            return;
        }

        if (opts.format == LsuserFormat::Json) {
            std::cout << "[\n";
            for (size_t i = 0; i < accounts.size(); ++i) {
                const auto& a = accounts[i];
                std::cout << "  {\"account\":\"" << JsonEscape(ToUtf8(a.name)) << "\","
                          << "\"uid\":" << a.uid << ","
                          << "\"groups\":\"" << JsonEscape(ToUtf8(a.groups)) << "\","
                          << "\"login\":\"" << JsonEscape(ToUtf8(a.loginDuration)) << "\","
                          << "\"status\":\"" << JsonEscape(ToUtf8(a.status)) << "\"}"
                          << (i + 1 == accounts.size() ? "\n" : ",\n");
            }
            std::cout << "]\n";
            return;
        }

        std::wcout << std::left;
        for (size_t i = 0; i < opts.requestedAttributes.size(); ++i) {
            const auto& attribute = opts.requestedAttributes[i];
            std::wcout << std::setw(GetColumnWidth(attribute)) << UserAccountEnumerator::ToWide(attribute)
                       << (i + 1 < opts.requestedAttributes.size() ? L"  " : L"");
        }
        std::wcout << std::endl;

        for (size_t i = 0; i < opts.requestedAttributes.size(); ++i) {
            const auto& attribute = opts.requestedAttributes[i];
            std::wcout << std::wstring(static_cast<size_t>(GetColumnWidth(attribute)), L'-')
                       << (i + 1 < opts.requestedAttributes.size() ? L"  " : L"");
        }
        std::wcout << std::endl;

        for (const auto& account : accounts) {
            for (size_t i = 0; i < opts.requestedAttributes.size(); ++i) {
                const auto& attribute = opts.requestedAttributes[i];
                std::wcout << std::setw(GetColumnWidth(attribute)) << GetDisplayValue(account, attribute)
                           << (i + 1 < opts.requestedAttributes.size() ? L"  " : L"");
            }
            std::wcout << std::endl;
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class LsuserApplication {
public:
    int Run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);
        std::wcout.imbue(std::locale(""));

        LsuserOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : "lsuser");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : "lsuser");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        std::vector<AccountRecord> accounts = UserAccountEnumerator::EnumerateLocalAccounts();
        if (accounts.empty()) {
            std::wcerr << L"lsuser: unable to enumerate local accounts" << std::endl;
            return 1;
        }

        if (!opts.requestedUsers.empty()) {
            std::vector<AccountRecord> filtered;
            for (const auto& account : accounts) {
                std::wstring lowerName = UserAccountEnumerator::ToLower(account.name);
                for (const auto& wanted : opts.requestedUsers) {
                    if (lowerName == UserAccountEnumerator::ToLower(wanted)) {
                        filtered.push_back(account);
                        break;
                    }
                }
            }
            accounts.swap(filtered);
        }

        if (accounts.empty()) {
            return 0;
        }

        LsuserReporter::Emit(opts, accounts);
        return 0;
    }
};

int main(int argc, char* argv[]) {
    LsuserApplication app;
    return app.Run(argc, argv);
}

