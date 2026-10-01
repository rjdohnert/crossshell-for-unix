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
#include <sddl.h>
#include <cstdio>
#include <memory>

#pragma comment(lib, "Advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. RAII HANDLES & GUARDS
// ============================================================================

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
    HLOCAL* Receive() { Close(); return &m_ptr; }
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
            FreeSid(m_sid);
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
// 2. PERMISSION & ACL MANAGERS
// ============================================================================

class AccessMaskCalculator {
public:
    static DWORD OctalToAccessMask(int digit, bool isDirectory) {
        DWORD mask = 0;

        if (digit & 4) {
            mask |= FILE_GENERIC_READ;
        }
        if (digit & 2) {
            mask |= FILE_GENERIC_WRITE | DELETE;
        }
        if (digit & 1) {
            mask |= FILE_GENERIC_EXECUTE;
        }

        if (isDirectory && (digit & 1)) {
            mask |= FILE_LIST_DIRECTORY | FILE_TRAVERSE;
        }

        return mask;
    }
};

class ModeParser {
public:
    static bool Parse(const std::wstring& modeStr, int& userMode, int& groupMode, int& otherMode, std::wostream& err) {
        userMode = 0;
        groupMode = 0;
        otherMode = 0;

        if (modeStr.length() >= 3 && modeStr.find_first_not_of(L"01234567") == std::wstring::npos) {
            std::wstring s = modeStr;
            if (s.length() == 4) s = s.substr(1);

            userMode = s[0] - L'0';
            groupMode = s[1] - L'0';
            otherMode = s[2] - L'0';
            return true;
        }

        std::wstring spec = modeStr;
        if (spec.empty()) {
            err << L"Error: Unsupported mode '" << modeStr << L"'\n";
            return false;
        }

        auto apply_permission = [](int& target, wchar_t ch, bool add, bool remove) {
            switch (ch) {
                case L'r': add ? (target |= 4) : (target &= ~4); break;
                case L'w': add ? (target |= 2) : (target &= ~2); break;
                case L'x': add ? (target |= 1) : (target &= ~1); break;
                case L'X': add ? (target |= 1) : (target &= ~1); break;
                default: break;
            }
        };

        auto apply_clause = [&](const std::wstring& clause, bool add, bool remove, bool set) -> bool {
            if (clause.empty()) return false;

            size_t opPos = std::wstring::npos;
            for (size_t i = 0; i < clause.size(); ++i) {
                if (clause[i] == L'+' || clause[i] == L'-' || clause[i] == L'=') {
                    opPos = i;
                    break;
                }
            }
            if (opPos == std::wstring::npos || opPos + 1 >= clause.size()) {
                if (!(set && opPos + 1 == clause.size())) {
                    return false;
                }
            }

            std::wstring who = clause.substr(0, opPos);
            std::wstring perms = (opPos + 1 < clause.size()) ? clause.substr(opPos + 1) : L"";
            if (who.empty()) who = L"a";

            std::vector<int*> targets;
            if (who.find(L'u') != std::wstring::npos || who == L"a") {
                targets.push_back(&userMode);
            }
            if (who.find(L'g') != std::wstring::npos || who == L"a") {
                targets.push_back(&groupMode);
            }
            if (who.find(L'o') != std::wstring::npos || who == L"a") {
                targets.push_back(&otherMode);
            }
            if (targets.empty()) {
                return false;
            }

            for (int* target : targets) {
                if (set) {
                    *target = 0;
                }
                for (wchar_t ch : perms) {
                    if (add) {
                        apply_permission(*target, ch, true, false);
                    } else if (remove) {
                        apply_permission(*target, ch, false, true);
                    } else {
                        apply_permission(*target, ch, false, false);
                    }
                }
            }
            return true;
        };

        std::vector<std::wstring> clauses;
        size_t start = 0;
        while (start <= spec.size()) {
            size_t comma = spec.find(L',', start);
            if (comma == std::wstring::npos) {
                clauses.push_back(spec.substr(start));
                break;
            }
            clauses.push_back(spec.substr(start, comma - start));
            start = comma + 1;
        }

        bool parsed = false;
        for (const auto& clause : clauses) {
            if (clause.empty()) continue;
            bool add = false;
            bool remove = false;
            bool set = false;
            if (clause.find(L'+') != std::wstring::npos) {
                add = true;
            } else if (clause.find(L'-') != std::wstring::npos) {
                remove = true;
            } else if (clause.find(L'=') != std::wstring::npos) {
                set = true;
            } else {
                err << L"Error: Unsupported mode '" << modeStr << L"'\n";
                return false;
            }

            if (!apply_clause(clause, add, remove, set)) {
                err << L"Error: Unsupported mode '" << modeStr << L"'\n";
                return false;
            }
            parsed = true;
        }

        if (!parsed) {
            err << L"Error: Unsupported mode '" << modeStr << L"'\n";
            return false;
        }

        return true;
    }
};

class AclPermissionManager {
public:
    static bool ApplyUnixPermissions(const fs::path& path, int uMode, int gMode, int oMode, std::wostream& err) {
        std::wstring pathStr = path.wstring();
        bool isDir = fs::is_directory(path);

        PSECURITY_DESCRIPTOR rawSD = NULL;
        PSID pOwnerSid = NULL;
        PSID pGroupSid = NULL;

        DWORD dwRes = GetNamedSecurityInfoW(
            pathStr.c_str(),
            SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
            &pOwnerSid,
            &pGroupSid,
            NULL, NULL,
            &rawSD
        );

        ScopedLocalAlloc pSD(rawSD);
        if (dwRes != ERROR_SUCCESS) {
            err << L"Failed to read Security Info for: " << pathStr 
                << L" (Error: " << dwRes << L")\n";
            return false;
        }

        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_WORLD_SID_AUTHORITY;
        PSID rawEveryoneSid = NULL;
        if (!AllocateAndInitializeSid(&NtAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &rawEveryoneSid)) {
            err << L"Failed to allocate Everyone SID. Error: " << GetLastError() << L"\n";
            return false;
        }
        ScopedSid pEveryoneSid(rawEveryoneSid);

        EXPLICIT_ACCESS_W ea[3] = { 0 };
        DWORD inheritance = isDir ? (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : NO_INHERITANCE;

        ea[0].grfAccessPermissions = AccessMaskCalculator::OctalToAccessMask(uMode, isDir);
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = inheritance;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_USER;
        ea[0].Trustee.ptstrName = reinterpret_cast<LPWSTR>(pOwnerSid);

        int entryCount = 1;

        if (pGroupSid != NULL && IsValidSid(pGroupSid)) {
            ea[entryCount].grfAccessPermissions = AccessMaskCalculator::OctalToAccessMask(gMode, isDir);
            ea[entryCount].grfAccessMode = SET_ACCESS;
            ea[entryCount].grfInheritance = inheritance;
            ea[entryCount].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ea[entryCount].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
            ea[entryCount].Trustee.ptstrName = reinterpret_cast<LPWSTR>(pGroupSid);
            entryCount++;
        }

        ea[entryCount].grfAccessPermissions = AccessMaskCalculator::OctalToAccessMask(oMode, isDir);
        ea[entryCount].grfAccessMode = SET_ACCESS;
        ea[entryCount].grfInheritance = inheritance;
        ea[entryCount].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[entryCount].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[entryCount].Trustee.ptstrName = reinterpret_cast<LPWSTR>(pEveryoneSid.Get());
        entryCount++;

        PACL rawNewDacl = NULL;
        dwRes = SetEntriesInAclW(entryCount, ea, NULL, &rawNewDacl);
        ScopedLocalAlloc pNewDacl(rawNewDacl);
        if (dwRes != ERROR_SUCCESS) {
            err << L"Failed to build DACL for: " << pathStr << L"\n";
            return false;
        }

        dwRes = SetNamedSecurityInfoW(
            const_cast<LPWSTR>(pathStr.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            NULL, NULL, reinterpret_cast<PACL>(pNewDacl.Get()), NULL
        );

        DWORD fileAttrs = GetFileAttributesW(pathStr.c_str());
        if (fileAttrs != INVALID_FILE_ATTRIBUTES) {
            if ((uMode & 2) == 0) {
                fileAttrs |= FILE_ATTRIBUTE_READONLY;
            } else {
                fileAttrs &= ~FILE_ATTRIBUTE_READONLY;
            }
            SetFileAttributesW(pathStr.c_str(), fileAttrs);
        }

        if (dwRes != ERROR_SUCCESS) {
            err << L"Failed to set DACL for " << pathStr << L" (Error: " << dwRes << L")\n";
            return false;
        }

        return true;
    }

    static bool ProcessPath(const fs::path& target, const std::wstring& modeStr, std::wostream& err) {
        int userMode = 0, groupMode = 0, otherMode = 0;
        if (!ModeParser::Parse(modeStr, userMode, groupMode, otherMode, err)) {
            return false;
        }

        return ApplyUnixPermissions(target, userMode, groupMode, otherMode, err);
    }
};

// ============================================================================
// 3. OPTIONS & REPORTER
// ============================================================================

class ChmodOptions {
public:
    bool recursive = false;
    bool verbose = false;
    bool changes = false;
    bool quiet = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipeCommand;
    std::wstring modeStr;
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
            if (modeStr.empty()) {
                modeStr = arg;
            } else {
                targets.push_back(arg);
            }
        }

        if (modeStr.empty() || targets.empty()) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
           std::wcout << LR"HELP(chmod(1)                 CrossShell for UNIX Reference Manual                   chmod(1)

    NAME
        chmod - change Windows file and directory permissions

    SYNOPSIS
        chmod [OPTIONS] MODE FILE...

    DESCRIPTION
        Applies numeric or symbolic permission modes to files and directories.
        Symbolic modes support u, g, o, a and r, w, x, X with +, -, and = clauses.

    OPTIONS
        -c, --changes          Report only when a change is made.
        -f, --silent, --quiet  Suppress most errors.
        -R, -r, --recursive    Process directories recursively.
        -v, --verbose          Report every processed file.
        --json, --csv, --table Select output format.
        --pipe COMMAND         Send structured output through COMMAND.
        -h, --help, /?         Display this comprehensive reference manual.
        --version              Display version information and exit.
        --                     End options before target paths.

    EXAMPLES
        chmod 755 script.bat
        chmod 600 secret.txt
        chmod -R 644 C:\MyFolder

    EXIT STATUS
        0          Help, version, or successful processing.
        1          Invalid mode, missing path, security, or target failure.

    CrossShell for UNIX                                                       chmod(1)
    )HELP";
           return;

        std::wcout << L"chmod\n";
        std::wcout << L"Usage: " << (exe ? exe : L"chmod") << L" [OPTION]... MODE[,MODE]... FILE...\n\n";
        std::wcout << L"Change the access permissions of each FILE.\n\n";
        std::wcout << L"Options:\n";
        std::wcout << L"  -c, --changes       report only when a change is made\n";
        std::wcout << L"  -f, --silent        suppress most error messages\n";
        std::wcout << L"  -R, --recursive     recurse into directories\n";
        std::wcout << L"  -v, --verbose       output a diagnostic for every file processed\n";
        std::wcout << L"      --help          display this help and exit\n";
        std::wcout << L"      --version       output version information and exit\n";
        std::wcout << L"      --json          output operation status as JSON\n"
                   << L"      --csv           output operation status as CSV\n"
                   << L"      --table         output operation status as a table\n"
                   << L"      --pipe COMMAND  send output through COMMAND\n\n";
        std::wcout << L"Examples:\n";
        std::wcout << L"  chmod 755 script.bat      (User: rwx, Group: r-x, Others: r-x)\n";
        std::wcout << L"  chmod 600 secret.txt      (User: rw-, Group: ---, Others: ---)\n";
        std::wcout << L"  chmod -R 644 C:\\MyFolder  (Recursive execution)\n";
    }

    void PrintVersion() const {
        std::wcout << L"chmod v1.0.0\n";
    }
};

class ChmodReporter {
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

class ChmodApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        ChmodOptions options;
        if (!options.Parse(argc, argv)) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chmod");
            return options.showHelp ? 0 : 1;
        }

        if (options.showHelp) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chmod");
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        bool success = true;

        for (const auto& targetPath : options.targets) {
            fs::path target(targetPath);
            if (!fs::exists(target)) {
                if (!options.quiet) {
                    std::wcerr << L"Error: Path does not exist: " << targetPath << L"\n";
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
                    if (!AclPermissionManager::ProcessPath(entry.path(), options.modeStr, std::wcerr)) {
                        success = false;
                    } else {
                        applied = true;
                    }
                }
                if (!AclPermissionManager::ProcessPath(target, options.modeStr, std::wcerr)) {
                    success = false;
                } else {
                    applied = true;
                }
            } else {
                if (AclPermissionManager::ProcessPath(target, options.modeStr, std::wcerr)) {
                    applied = true;
                } else {
                    success = false;
                }
            }

            if ((options.verbose || options.changes) && applied) {
                std::wcout << L"mode of '" << targetPath << L"' changed to '" << options.modeStr << L"'\n";
            }
        }

        ChmodReporter::OutputResults(success, options.format, options.pipeCommand);
        return success ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ChmodApplication app;
    return app.Run(argc, argv);
}
