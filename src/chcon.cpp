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

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cwctype>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <cstdio>
#include <memory>

#pragma comment(lib, "Advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. RAII HANDLES & GUARDS
// ============================================================================

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
    HANDLE* Receive() { Close(); return &m_handle; }
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

class ScopedSid {
public:
    explicit ScopedSid(PSID sid = NULL) : m_sid(sid) {}

    ~ScopedSid() {
        Close();
    }

    ScopedSid(const ScopedSid&) = delete;
    ScopedSid& operator=(const ScopedSid&) = delete;

    ScopedSid(ScopedSid&& other) noexcept : m_sid(other.m_sid) {
        other.m_sid = NULL;
    }

    ScopedSid& operator=(ScopedSid&& other) noexcept {
        if (this != &other) {
            Close();
            m_sid = other.m_sid;
            other.m_sid = NULL;
        }
        return *this;
    }

    PSID Get() const { return m_sid; }
    PSID* Receive() { Close(); return &m_sid; }
    bool IsValid() const { return m_sid != NULL; }

    void Close() {
        if (m_sid != NULL) {
            LocalFree(m_sid);
            m_sid = NULL;
        }
    }

private:
    PSID m_sid;
};

class ScopedLocalAlloc {
public:
    explicit ScopedLocalAlloc(HLOCAL ptr = NULL) : m_ptr(ptr) {}

    ~ScopedLocalAlloc() {
        Close();
    }

    ScopedLocalAlloc(const ScopedLocalAlloc&) = delete;
    ScopedLocalAlloc& operator=(const ScopedLocalAlloc&) = delete;

    ScopedLocalAlloc(ScopedLocalAlloc&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = NULL;
    }

    ScopedLocalAlloc& operator=(ScopedLocalAlloc&& other) noexcept {
        if (this != &other) {
            Close();
            m_ptr = other.m_ptr;
            other.m_ptr = NULL;
        }
        return *this;
    }

    HLOCAL Get() const { return m_ptr; }
    bool IsValid() const { return m_ptr != NULL; }

    void Close() {
        if (m_ptr != NULL) {
            LocalFree(m_ptr);
            m_ptr = NULL;
        }
    }

private:
    HLOCAL m_ptr;
};

enum class OutputFormat {
    Default,
    Json,
    Csv,
    Table
};

// ============================================================================
// 2. PRIVILEGE & SECURITY CONTEXT MANAGERS
// ============================================================================

class PrivilegeManager {
public:
    static bool EnablePrivilege(LPCWSTR lpszPrivilege) {
        ScopedTokenHandle hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, hToken.Receive())) {
            return false;
        }

        TOKEN_PRIVILEGES tp = {};
        LUID luid = {};

        if (!LookupPrivilegeValueW(NULL, lpszPrivilege, &luid)) {
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        return AdjustTokenPrivileges(hToken.Get(), FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) &&
               (GetLastError() == ERROR_SUCCESS);
    }

    static void EnableRequiredPrivileges() {
        EnablePrivilege(L"SeSecurityPrivilege");
        EnablePrivilege(L"SeRestorePrivilege");
        EnablePrivilege(L"SeTakeOwnershipPrivilege");
    }
};

class SecurityContextMapper {
public:
    static std::wstring LevelToStringSid(const std::wstring& levelStr) {
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

class IntegrityLevelManager {
public:
    static bool SetIntegrityLevel(const fs::path& path, const std::wstring& stringSid, std::wostream& err) {
        ScopedSid pIntegritySid;
        if (!ConvertStringSidToSidW(stringSid.c_str(), pIntegritySid.Receive())) {
            err << L"chcon: Invalid security context / SID: " << stringSid << L"\n";
            return false;
        }

        DWORD aclSize = sizeof(ACL) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) + GetLengthSid(pIntegritySid.Get()) - sizeof(DWORD);
        ScopedLocalAlloc pSacl(LocalAlloc(LPTR, aclSize));
        if (!pSacl.IsValid()) {
            return false;
        }

        PACL acl = reinterpret_cast<PACL>(pSacl.Get());
        if (!InitializeAcl(acl, aclSize, ACL_REVISION)) {
            return false;
        }

        if (!AddMandatoryAce(acl, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, pIntegritySid.Get())) {
            return false;
        }

        std::wstring pathStr = path.wstring();

        DWORD dwRes = SetNamedSecurityInfoW(
            const_cast<LPWSTR>(pathStr.c_str()),
            SE_FILE_OBJECT,
            LABEL_SECURITY_INFORMATION,
            NULL,
            NULL,
            NULL,
            acl
        );

        if (dwRes != ERROR_SUCCESS) {
            err << L"chcon: " << pathStr << L": Failed to set security context (Error Code: " << dwRes << L")\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 3. OPTIONS & REPORTER
// ============================================================================

class ChconOptions {
public:
    bool recursive = false;
    bool verbose = false;
    bool changes = false;
    bool quiet = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipeCommand;
    std::wstring contextStr;
    std::vector<std::wstring> targets;

    bool Parse(int argc, wchar_t* argv[]) {
        if (argc < 2) {
            showHelp = true;
            return false;
        }

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == L"--") {
                for (int j = i + 1; j < argc; ++j) {
                    targets.push_back(argv[j]);
                }
                break;
            }
            if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                showHelp = true;
                return true;
            }
            if (arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-R" || arg == L"-r" || arg == L"--recursive") {
                recursive = true;
                continue;
            }
            if (arg == L"-c" || arg == L"--changes") {
                changes = true;
                continue;
            }
            if (arg == L"-v" || arg == L"--verbose") {
                verbose = true;
                continue;
            }
            if (arg == L"-f" || arg == L"--silent" || arg == L"--quiet") {
                quiet = true;
                continue;
            }
            if (contextStr.empty()) {
                contextStr = arg;
            } else {
                targets.push_back(arg);
            }
        }

        if (contextStr.empty() || targets.empty()) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
        std::wcout << LR"(chcon(1)                CrossShell for UNIX Reference Manual                 chcon(1)

    NAME
        chcon - change file security context and integrity labels

    SYNOPSIS
        chcon [OPTIONS] CONTEXT FILE...
        chcon [OPTIONS] --reference=RFILE FILE...

    DESCRIPTION
        chcon changes the security context, DACL, or integrity level of each
        FILE according to CONTEXT or from a reference file.

    OPTIONS
        -R, --recursive
            Operate on files and directories recursively.

        --reference=RFILE
            Use RFILE's security context rather than specifying CONTEXT.

        -v, --verbose
            Output a diagnostic for every file processed.

        --json, --csv, --table
            Format result records as JSON, CSV, or table.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        chcon HighIntegrity C:\Protected\app.exe
            Apply High integrity label to executable.

    CrossShell for UNIX                                                  chcon(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"chcon v1.0.0\n";
    }
};

class ChconReporter {
public:
    static std::string ToUtf8(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static void OutputResults(bool success, OutputFormat format, const std::wstring& pipeCommand) {
        if (format == OutputFormat::Default && pipeCommand.empty()) return;

        std::wstring text;
        if (format == OutputFormat::Json) {
            text = L"{\"status\":\"" + std::wstring(success ? L"success" : L"failure") + L"\"}\n";
        } else if (format == OutputFormat::Csv) {
            text = L"status\n" + std::wstring(success ? L"success\n" : L"failure\n");
        } else {
            text = L"STATUS\n" + std::wstring(success ? L"success\n" : L"failure\n");
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (pipe) {
                std::string narrow = ToUtf8(text);
                fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            }
        } else {
            std::wcout << text;
        }
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ChconApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        ChconOptions options;
        if (!options.Parse(argc, argv)) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chcon");
            return options.showHelp ? 0 : 1;
        }

        if (options.showHelp) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chcon");
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        PrivilegeManager::EnableRequiredPrivileges();

        std::wstring stringSid = SecurityContextMapper::LevelToStringSid(options.contextStr);
        if (stringSid.empty()) {
            if (!options.quiet) {
                std::wcerr << L"chcon: Invalid security context: " << options.contextStr << L"\n";
            }
            return 1;
        }

        bool success = true;

        for (const auto& targetPath : options.targets) {
            fs::path target(targetPath);
            if (!fs::exists(target)) {
                if (!options.quiet) {
                    std::wcerr << L"chcon: " << targetPath << L": No such file or directory\n";
                }
                success = false;
                continue;
            }

            bool applied = false;
            if (options.recursive && fs::is_directory(target)) {
                std::error_code ec;
                auto iter = fs::recursive_directory_iterator(
                    target,
                    fs::directory_options::skip_permission_denied,
                    ec
                );

                for (auto& entry : iter) {
                    if (!IntegrityLevelManager::SetIntegrityLevel(entry.path(), stringSid, std::wcerr)) {
                        success = false;
                    } else {
                        applied = true;
                    }
                }
                if (!IntegrityLevelManager::SetIntegrityLevel(target, stringSid, std::wcerr)) {
                    success = false;
                } else {
                    applied = true;
                }
            } else {
                if (IntegrityLevelManager::SetIntegrityLevel(target, stringSid, std::wcerr)) {
                    applied = true;
                } else {
                    success = false;
                }
            }

            if ((options.verbose || options.changes) && applied && !options.quiet) {
                std::wcout << L"changed context of '" << targetPath << L"' to '" << options.contextStr << L"'\n";
            }
        }

        ChconReporter::OutputResults(success, options.format, options.pipeCommand);
        return success ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ChconApplication app;
    return app.Run(argc, argv);
}
