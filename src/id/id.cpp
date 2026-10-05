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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <lm.h>

#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <cstdio>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Netapi32.lib")

// ============================================================================
// 1. DATA MODELS & RAII HANDLERS
// ============================================================================

class NativePipeConfigurator {
public:
    static void Configure() {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode = 0;
        const bool stdoutConsole = (GetFileType(stdoutHandle) == FILE_TYPE_CHAR) && GetConsoleMode(stdoutHandle, &consoleMode);
        _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
        _setmode(_fileno(stderr), _O_U8TEXT);
    }
};

class ScopedTokenHandle {
public:
    explicit ScopedTokenHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedTokenHandle() {
        Close();
    }

    ScopedTokenHandle(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle& operator=(const ScopedTokenHandle&) = delete;

    ScopedTokenHandle(ScopedTokenHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedTokenHandle& operator=(ScopedTokenHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    operator HANDLE() const { return m_handle; }
    HANDLE* ReceiveHandle() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

struct AccountInfo {
    std::wstring sidString;
    std::wstring accountName;
};

struct UserTokenData {
    AccountInfo user;
    AccountInfo primaryGroup;
    std::vector<AccountInfo> groups;
};

// ============================================================================
// 2. SID RESOLVER & IDENTITY RETRIEVER
// ============================================================================

class SidResolver {
public:
    static AccountInfo Resolve(PSID pSid) {
        AccountInfo info;
        if (!pSid || !IsValidSid(pSid)) return info;

        LPWSTR strSid = nullptr;
        if (ConvertSidToStringSidW(pSid, &strSid)) {
            info.sidString = strSid;
            LocalFree(strSid);
        }

        wchar_t name[256] = { 0 };
        wchar_t domain[256] = { 0 };
        DWORD nameSize = 256;
        DWORD domainSize = 256;
        SID_NAME_USE sidUse;

        if (LookupAccountSidW(nullptr, pSid, name, &nameSize, domain, &domainSize, &sidUse)) {
            if (domainSize > 0 && wcslen(domain) > 0) {
                info.accountName = std::wstring(domain) + L"\\" + std::wstring(name);
            } else {
                info.accountName = name;
            }
        } else {
            info.accountName = info.sidString;
        }

        return info;
    }
};

class IdentityEngine {
public:
    static UserTokenData GetCurrentProcessData() {
        UserTokenData data;
        ScopedTokenHandle hToken;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, hToken.ReceiveHandle())) {
            return data;
        }

        DWORD len = 0;
        GetTokenInformation(hToken.Get(), TokenUser, nullptr, 0, &len);
        if (len > 0) {
            std::vector<BYTE> buffer(len);
            if (GetTokenInformation(hToken.Get(), TokenUser, buffer.data(), len, &len)) {
                PTOKEN_USER pUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                data.user = SidResolver::Resolve(pUser->User.Sid);
            }
        }

        len = 0;
        GetTokenInformation(hToken.Get(), TokenPrimaryGroup, nullptr, 0, &len);
        if (len > 0) {
            std::vector<BYTE> buffer(len);
            if (GetTokenInformation(hToken.Get(), TokenPrimaryGroup, buffer.data(), len, &len)) {
                PTOKEN_PRIMARY_GROUP pGroup = reinterpret_cast<PTOKEN_PRIMARY_GROUP>(buffer.data());
                data.primaryGroup = SidResolver::Resolve(pGroup->PrimaryGroup);
            }
        }

        len = 0;
        GetTokenInformation(hToken.Get(), TokenGroups, nullptr, 0, &len);
        if (len > 0) {
            std::vector<BYTE> buffer(len);
            if (GetTokenInformation(hToken.Get(), TokenGroups, buffer.data(), len, &len)) {
                PTOKEN_GROUPS pGroups = reinterpret_cast<PTOKEN_GROUPS>(buffer.data());
                for (DWORD i = 0; i < pGroups->GroupCount; ++i) {
                    data.groups.push_back(SidResolver::Resolve(pGroups->Groups[i].Sid));
                }
            }
        }

        return data;
    }

    static UserTokenData GetUserDataByName(const std::wstring& username) {
        UserTokenData data;
        DWORD sidSize = 0, domainSize = 0;
        SID_NAME_USE use;

        LookupAccountNameW(nullptr, username.c_str(), nullptr, &sidSize, nullptr, &domainSize, &use);
        if (sidSize == 0) return data;

        std::vector<BYTE> sidBuf(sidSize);
        std::vector<wchar_t> domainBuf(domainSize);

        if (!LookupAccountNameW(nullptr, username.c_str(), sidBuf.data(), &sidSize, domainBuf.data(), &domainSize, &use)) {
            return data;
        }

        data.user = SidResolver::Resolve(sidBuf.data());

        LPLOCALGROUP_USERS_INFO_0 pGroupInfo = nullptr;
        DWORD entriesRead = 0, totalEntries = 0;
        NET_API_STATUS status = NetUserGetLocalGroups(
            nullptr, username.c_str(), 0, LG_INCLUDE_INDIRECT,
            reinterpret_cast<LPBYTE*>(&pGroupInfo), MAX_PREFERRED_LENGTH,
            &entriesRead, &totalEntries
        );

        if (status == NERR_Success && pGroupInfo) {
            for (DWORD i = 0; i < entriesRead; ++i) {
                DWORD gSidSize = 0, gDomSize = 0;
                SID_NAME_USE gUse;
                LookupAccountNameW(nullptr, pGroupInfo[i].lgrui0_name, nullptr, &gSidSize, nullptr, &gDomSize, &gUse);
                if (gSidSize > 0) {
                    std::vector<BYTE> gSidBuf(gSidSize);
                    std::vector<wchar_t> gDomBuf(gDomSize);
                    if (LookupAccountNameW(nullptr, pGroupInfo[i].lgrui0_name, gSidBuf.data(), &gSidSize, gDomBuf.data(), &gDomSize, &gUse)) {
                        data.groups.push_back(SidResolver::Resolve(gSidBuf.data()));
                    }
                }
            }
            NetApiBufferFree(pGroupInfo);
        }

        if (!data.groups.empty()) {
            data.primaryGroup = data.groups[0];
        } else {
            data.primaryGroup = data.user;
        }

        return data;
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class IdOptions {
public:
    bool optUser = false;
    bool optGroup = false;
    bool optGroups = false;
    bool optName = false;
    std::wstring targetUser;
    OutputFormat format = OutputFormat::Standard;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == L"-u" || arg == L"--user") optUser = true;
            else if (arg == L"-g" || arg == L"--group") optGroup = true;
            else if (arg == L"-G" || arg == L"--groups") optGroups = true;
            else if (arg == L"-n" || arg == L"--name") optName = true;
            else if (arg == L"-r" || arg == L"--real") { /* POSIX compatibility flag */ }
            else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                showHelp = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            } else if (arg == L"--") {
                if (i + 1 < argc && targetUser.empty()) targetUser = argv[++i];
                break;
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"id: invalid option -- " << arg << L"\n";
                return false;
            } else if (targetUser.empty()) {
                targetUser = arg;
            } else {
                std::wcerr << L"id: extra operand '" << arg << L"'\n";
                return false;
            }
        }

        if (optName && !optUser && !optGroup && !optGroups) {
            std::wcerr << L"id: cannot print only names in default format\n";
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
        std::wcout << LR"(id(1)                   CrossShell for UNIX Reference Manual                    id(1)

    NAME
        id - print real and effective user and group IDs and SIDs

    SYNOPSIS
        id [OPTIONS] [USER]

    DESCRIPTION
        id prints user and group information for the current user, or for the
        specified USER on the local Windows machine or domain.

    OPTIONS
        -u, --user
            Print only the effective user ID / SID.

        -g, --group
            Print only the effective group ID / SID.

        -G, --groups
            Print all group IDs / SIDs.

        -n, --name
            Print a name instead of a number/SID for -u, -g, -G.

        -r, --real
            Print the real ID instead of the effective ID.

        --json, --csv, --table
            Format user and group identity data as JSON, CSV, or table.

        --pipe COMMAND
            Stream output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        id
            Print full identity information for current session.

        id -Gn Administrator
            Print all group names for Administrator.

    CrossShell for UNIX                                                     id(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"id 1.0.0\n";
    }
};

// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class IdReporter {
public:
    static void Emit(const IdOptions& opts, const UserTokenData& data) {
        std::wostringstream output;
        if (opts.optUser) {
            output << (opts.optName ? data.user.accountName : data.user.sidString) << L"\n";
        } else if (opts.optGroup) {
            output << (opts.optName ? data.primaryGroup.accountName : data.primaryGroup.sidString) << L"\n";
        } else if (opts.optGroups) {
            for (size_t i = 0; i < data.groups.size(); ++i) {
                output << (opts.optName ? data.groups[i].accountName : data.groups[i].sidString);
                if (i + 1 < data.groups.size()) output << L" ";
            }
            output << L"\n";
        } else {
            output << L"uid=" << data.user.sidString << L"(" << data.user.accountName << L") ";
            output << L"gid=" << data.primaryGroup.sidString << L"(" << data.primaryGroup.accountName << L") ";
            output << L"groups=";
            for (size_t i = 0; i < data.groups.size(); ++i) {
                output << data.groups[i].sidString << L"(" << data.groups[i].accountName << L")";
                if (i + 1 < data.groups.size()) output << L",";
            }
            output << L"\n";
        }

        std::wstring raw = output.str();
        std::wstring formatted;
        if (opts.format == OutputFormat::Json) {
            formatted = L"{\"output\":\"" + EscapeJson(raw) + L"\"}\n";
        } else if (opts.format == OutputFormat::Csv) {
            formatted = L"output\n\"" + raw + L"\"\n";
        } else if (opts.format == OutputFormat::Table) {
            formatted = L"OUTPUT\n------\n" + raw;
        } else {
            formatted = raw;
        }

        if (!opts.pipeCommand.empty()) {
            SendToPipe(opts.pipeCommand, formatted);
        } else {
            std::wcout << formatted;
        }
    }

private:
    static std::wstring EscapeJson(const std::wstring& src) {
        std::wstring out;
        for (wchar_t ch : src) {
            if (ch == L'\\' || ch == L'"') {
                out.push_back(L'\\');
            } else if (ch == L'\n') {
                out += L"\\n";
                continue;
            } else if (ch == L'\r') {
                out += L"\\r";
                continue;
            }
            out.push_back(ch);
        }
        return out;
    }

    static void SendToPipe(const std::wstring& pipeCmd, const std::wstring& text) {
        FILE* pipe = _wpopen(pipeCmd.c_str(), L"w");
        if (!pipe) return;
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string narrow(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
            fwrite(narrow.data(), 1, narrow.size(), pipe);
        }
        _pclose(pipe);
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class IdApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        NativePipeConfigurator::Configure();

        IdOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"id");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"id");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        UserTokenData data = opts.targetUser.empty() ? IdentityEngine::GetCurrentProcessData()
                                                    : IdentityEngine::GetUserDataByName(opts.targetUser);

        if (data.user.sidString.empty()) {
            std::wcerr << L"id: '" << (opts.targetUser.empty() ? L"current user" : opts.targetUser) << L"': no such user\n";
            return 1;
        }

        IdReporter::Emit(opts, data);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    IdApplication app;
    return app.Run(argc, argv);
}
