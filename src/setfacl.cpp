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
#include <aclapi.h>
#include <sddl.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. DATA STRUCTURES & PRINCIPAL RESOLUTION
// ============================================================================

struct FaclEntry {
    std::wstring kind;
    std::wstring name;
    std::wstring rights;
};

class PrincipalResolver {
public:
    static std::wstring ToLower(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        return value;
    }

    static DWORD RightsToMask(const std::wstring& rights) {
        std::wstring normalized = ToLower(rights);
        if (normalized == L"rwx" || normalized == L"full" || normalized == L"u+rwx") {
            return FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE | WRITE_DAC | WRITE_OWNER;
        }

        DWORD mask = 0;
        if (normalized.find(L'r') != std::wstring::npos) mask |= FILE_GENERIC_READ;
        if (normalized.find(L'w') != std::wstring::npos) mask |= FILE_GENERIC_WRITE | DELETE;
        if (normalized.find(L'x') != std::wstring::npos) mask |= FILE_GENERIC_EXECUTE;
        return mask;
    }

    static std::wstring SidToName(PSID sid) {
        if (!sid) {
            return L"unknown";
        }

        wchar_t name[256] = {};
        wchar_t domain[256] = {};
        DWORD nameLen = 256;
        DWORD domainLen = 256;
        SID_NAME_USE sidType;
        if (LookupAccountSidW(nullptr, sid, name, &nameLen, domain, &domainLen, &sidType)) {
            if (domainLen > 0 && domain[0] != L'\0') {
                return std::wstring(domain) + L"\\" + name;
            }
            return name;
        }

        LPWSTR sidString = nullptr;
        if (ConvertSidToStringSidW(sid, &sidString)) {
            std::wstring result = sidString;
            LocalFree(sidString);
            return result;
        }
        return L"unknown";
    }

    static PSID NameToSid(const std::wstring& name, std::vector<BYTE>& sidBuffer, std::vector<wchar_t>& domainBuffer) {
        DWORD sidLen = 0;
        DWORD domainLen = 0;
        SID_NAME_USE sidType;
        LookupAccountNameW(nullptr, name.c_str(), nullptr, &sidLen, nullptr, &domainLen, &sidType);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || sidLen == 0) {
            return nullptr;
        }

        sidBuffer.resize(sidLen);
        domainBuffer.resize(domainLen > 0 ? domainLen : 1);
        if (!LookupAccountNameW(nullptr, name.c_str(), sidBuffer.data(), &sidLen, domainBuffer.data(), &domainLen, &sidType)) {
            return nullptr;
        }
        return reinterpret_cast<PSID>(sidBuffer.data());
    }

    static PSID AllocateEveryoneSid() {
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_WORLD_SID_AUTHORITY;
        PSID sid = nullptr;
        if (!AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &sid)) {
            return nullptr;
        }
        return sid;
    }
};

// ============================================================================
// 2. ACL ENGINE
// ============================================================================

class AclEngine {
public:
    static bool ApplyEntry(const std::wstring& path, const FaclEntry& entry) {
        std::vector<BYTE> sidBuf;
        std::vector<wchar_t> domainBuf;
        PSID sid = nullptr;
        bool isAllocatedSid = false;

        if (entry.kind == L"o") {
            sid = PrincipalResolver::AllocateEveryoneSid();
            if (!sid) {
                std::wcerr << L"setfacl: failed to resolve everyone SID for " << path << L"\n";
                return false;
            }
            isAllocatedSid = true;
        } else {
            sid = PrincipalResolver::NameToSid(entry.name, sidBuf, domainBuf);
            if (!sid) {
                std::wcerr << L"setfacl: could not resolve principal '" << entry.name << L"'\n";
                return false;
            }
        }

        EXPLICIT_ACCESS_W ea{};
        ea.grfAccessPermissions = PrincipalResolver::RightsToMask(entry.rights);
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = (entry.kind == L"g") ? TRUSTEE_IS_GROUP : TRUSTEE_IS_USER;
        ea.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

        PACL newDacl = nullptr;
        DWORD res = SetEntriesInAclW(1, &ea, nullptr, &newDacl);
        if (res != ERROR_SUCCESS) {
            if (isAllocatedSid) FreeSid(sid);
            std::wcerr << L"setfacl: failed to build ACL for '" << path << L"'\n";
            return false;
        }

        res = SetNamedSecurityInfoW(
            const_cast<LPWSTR>(path.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr,
            nullptr,
            newDacl,
            nullptr
        );

        if (newDacl) LocalFree(newDacl);
        if (isAllocatedSid) FreeSid(sid);

        if (res != ERROR_SUCCESS) {
            std::wcerr << L"setfacl: failed to apply ACL to '" << path << L"' (" << res << L")\n";
            return false;
        }

        std::wcout << L"setfacl: updated ACL for " << path << L"\n";
        return true;
    }

    static bool RemoveDacl(const std::wstring& path) {
        DWORD res = SetNamedSecurityInfoW(
            const_cast<LPWSTR>(path.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );

        if (res != ERROR_SUCCESS) {
            std::wcerr << L"setfacl: failed to clear ACL for '" << path << L"' (" << res << L")\n";
            return false;
        }

        std::wcout << L"setfacl: cleared ACL for " << path << L"\n";
        return true;
    }

    static bool ProcessDirectoryRecursive(const std::wstring& dirPath, bool removeDacl, const FaclEntry& entry) {
        std::wstring pattern = dirPath;
        if (!pattern.empty() && pattern.back() != L'\\') {
            pattern.push_back(L'\\');
        }
        pattern.push_back(L'*');

        WIN32_FIND_DATAW findData{};
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            return true;
        }

        bool ok = true;
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            std::wstring child = dirPath;
            if (!child.empty() && child.back() != L'\\') {
                child.push_back(L'\\');
            }
            child += findData.cFileName;

            if (removeDacl) {
                if (!RemoveDacl(child)) ok = false;
            } else {
                if (!ApplyEntry(child, entry)) ok = false;
            }

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!ProcessDirectoryRecursive(child, removeDacl, entry)) {
                    ok = false;
                }
            }
        } while (FindNextFileW(hFind, &findData));

        FindClose(hFind);
        return ok;
    }
};

// ============================================================================
// 3. OPTION PARSER & HELP SYSTEM
// ============================================================================

struct SetfaclOptions {
    bool recursive = false;
    bool removeDacl = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring spec;
    std::vector<std::wstring> paths;
};

class OptionParser {
public:
    static void PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" [OPTIONS] ACL_SPEC FILE...\n"
            << L"Apply a simple access control entry to Windows files.\n\n"
            << L"ACL syntax (baseline):\n"
            << L"  u:NAME:rwx   user entry\n"
            << L"  g:NAME:rwx   group entry\n"
            << L"  o::rwx       other/everyone entry\n\n"
            << L"Options:\n"
            << L"  -m, --modify   apply an ACL entry (default)\n"
            << L"  -b, --remove   remove the DACL (baseline clear)\n"
            << L"  -R, --recursive operate recursively on directories\n"
            << L"  -h, --help    display this help and exit\n"
            << L"  -V, --version output version information and exit\n"
            << L"  --            end of options\n";
    }

    static void PrintVersion() {
        std::wcout << L"setfacl 1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], SetfaclOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    opts.paths.push_back(argv[i] ? argv[i] : L"");
                }
                break;
            }
            if (arg == L"-h" || arg == L"--help") {
                opts.showHelp = true;
                return true;
            }
            if (arg == L"-V" || arg == L"--version") {
                opts.showVersion = true;
                return true;
            }
            if (arg == L"-R" || arg == L"--recursive") {
                opts.recursive = true;
                continue;
            }
            if (arg == L"-b" || arg == L"--remove") {
                opts.removeDacl = true;
                continue;
            }
            if (arg == L"-m" || arg == L"--modify") {
                continue;
            }
            if (opts.spec.empty() && !opts.removeDacl) {
                opts.spec = arg;
                continue;
            }
            opts.paths.push_back(arg);
        }

        if ((opts.spec.empty() && !opts.removeDacl) || opts.paths.empty()) {
            return false;
        }

        return true;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class SetfaclApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"setfacl";

        SetfaclOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            if (opts.showHelp) {
                OptionParser::PrintUsage(progName);
                return 0;
            }
            if (opts.showVersion) {
                OptionParser::PrintVersion();
                return 0;
            }
            OptionParser::PrintUsage(progName);
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintUsage(progName);
            return 0;
        }

        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        FaclEntry entry{};
        if (!opts.removeDacl) {
            size_t first = opts.spec.find(L':');
            size_t second = opts.spec.find(L':', first == std::wstring::npos ? first : first + 1);
            if (first == std::wstring::npos || second == std::wstring::npos) {
                std::wcerr << L"setfacl: invalid ACL spec '" << opts.spec << L"'\n";
                return 1;
            }

            entry.kind = opts.spec.substr(0, first);
            entry.name = opts.spec.substr(first + 1, second - first - 1);
            entry.rights = opts.spec.substr(second + 1);
        }

        bool ok = true;
        for (const auto& path : opts.paths) {
            DWORD attrs = GetFileAttributesW(path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                std::wcerr << L"setfacl: " << path << L": No such file or directory\n";
                ok = false;
                continue;
            }

            if (opts.removeDacl) {
                if (!AclEngine::RemoveDacl(path)) ok = false;
            } else {
                if (!AclEngine::ApplyEntry(path, entry)) ok = false;
            }

            if (opts.recursive && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                if (!AclEngine::ProcessDirectoryRecursive(path, opts.removeDacl, entry)) {
                    ok = false;
                }
            }
        }

        return ok ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SetfaclApplication app;
    return app.Run(argc, argv);
}
