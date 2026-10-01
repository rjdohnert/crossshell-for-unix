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
#include <windows.h>
#include <aclapi.h>
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
    bool IsValid() const { return m_sid != NULL; }

    void Reset(PSID sid = NULL) {
        Close();
        m_sid = sid;
    }

    void Close() {
        if (m_sid != NULL) {
            free(m_sid);
            m_sid = NULL;
        }
    }

private:
    PSID m_sid;
};

enum class OutputFormat {
    Default,
    Json,
    Csv,
    Table
};

// ============================================================================
// 2. PRIVILEGE & GROUP MANAGERS
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

class GroupSidResolver {
public:
    static ScopedSid GetGroupSid(const std::wstring& groupName) {
        DWORD sidSize = 0;
        DWORD domainSize = 0;
        SID_NAME_USE sidUse;

        LookupAccountNameW(NULL, groupName.c_str(), NULL, &sidSize, NULL, &domainSize, &sidUse);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return ScopedSid(NULL);
        }

        PSID pSid = reinterpret_cast<PSID>(malloc(sidSize));
        if (!pSid) return ScopedSid(NULL);

        std::vector<wchar_t> domainBuffer(domainSize);

        if (!LookupAccountNameW(NULL, groupName.c_str(), pSid, &sidSize, domainBuffer.data(), &domainSize, &sidUse)) {
            free(pSid);
            return ScopedSid(NULL);
        }

        return ScopedSid(pSid);
    }
};

class GroupManager {
public:
    static bool ApplyGroup(const fs::path& path, PSID pGroupSid, std::wostream& err) {
        std::wstring pathStr = path.wstring();

        DWORD dwRes = SetNamedSecurityInfoW(
            const_cast<LPWSTR>(pathStr.c_str()),
            SE_FILE_OBJECT,
            GROUP_SECURITY_INFORMATION,
            NULL,
            pGroupSid,
            NULL,
            NULL
        );

        if (dwRes != ERROR_SUCCESS) {
            err << L"chgrp: " << pathStr << L": Permission denied or failed (Error Code: " 
                << dwRes << L")\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 3. OPTIONS & REPORTER
// ============================================================================

class ChgrpOptions {
public:
    bool recursive = false;
    bool verbose = false;
    bool changes = false;
    bool quiet = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipeCommand;
    std::wstring groupName;
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
            if (groupName.empty()) {
                groupName = arg;
            } else {
                targets.push_back(arg);
            }
        }

        if (groupName.empty() || targets.empty()) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
        std::wcout << LR"(chgrp(1)                CrossShell for UNIX Reference Manual                 chgrp(1)

    NAME
        chgrp - change group ownership of files and directories

    SYNOPSIS
        chgrp [OPTIONS] GROUP FILE...
        chgrp [OPTIONS] --reference=RFILE FILE...

    DESCRIPTION
        chgrp changes the group ownership (primary group or group trustee in the
        security descriptor) of each FILE to GROUP.

    OPTIONS
        -R, --recursive
            Operate on files and directories recursively.

        --reference=RFILE
            Use RFILE's group rather than specifying a GROUP value.

        -v, --verbose
            Output a diagnostic for every file processed.

        --json, --csv, --table
            Output structured operation reports.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        chgrp Administrators data\
            Change group ownership of folder to Administrators.

    CrossShell for UNIX                                                  chgrp(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"chgrp v1.0.0\n";
    }
};

class ChgrpReporter {
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

class ChgrpApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        ChgrpOptions options;
        if (!options.Parse(argc, argv)) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chgrp");
            return options.showHelp ? 0 : 1;
        }

        if (options.showHelp) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chgrp");
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        PrivilegeManager::EnableRequiredPrivileges();

        ScopedSid pGroupSid = GroupSidResolver::GetGroupSid(options.groupName);
        if (!pGroupSid.IsValid()) {
            if (!options.quiet) {
                std::wcerr << L"chgrp: invalid group: '" << options.groupName << L"'\n";
            }
            return 1;
        }

        bool success = true;

        for (const auto& targetPath : options.targets) {
            fs::path target(targetPath);
            if (!fs::exists(target)) {
                if (!options.quiet) {
                    std::wcerr << L"chgrp: " << targetPath << L": No such file or directory\n";
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
                    if (!GroupManager::ApplyGroup(entry.path(), pGroupSid.Get(), std::wcerr)) {
                        success = false;
                    } else {
                        applied = true;
                    }
                }
                if (!GroupManager::ApplyGroup(target, pGroupSid.Get(), std::wcerr)) {
                    success = false;
                } else {
                    applied = true;
                }
            } else {
                if (GroupManager::ApplyGroup(target, pGroupSid.Get(), std::wcerr)) {
                    applied = true;
                } else {
                    success = false;
                }
            }

            if ((options.verbose || options.changes) && applied && !options.quiet) {
                std::wcout << L"changed group of '" << targetPath << L"' to " << options.groupName << L"\n";
            }
        }

        ChgrpReporter::OutputResults(success, options.format, options.pipeCommand);
        return success ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ChgrpApplication app;
    return app.Run(argc, argv);
}
