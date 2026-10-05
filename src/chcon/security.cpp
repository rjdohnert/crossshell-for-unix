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

#include "security.hpp"
#include <cwctype>

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#pragma comment(lib, "Advapi32.lib")
#endif

namespace fs = std::filesystem;

bool PrivilegeManager::EnablePrivilege(const wchar_t* lpszPrivilege) {
#ifdef _WIN32
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
#else
    (void)lpszPrivilege;
    return true;
#endif
}

void PrivilegeManager::EnableRequiredPrivileges() {
#ifdef _WIN32
    EnablePrivilege(L"SeSecurityPrivilege");
    EnablePrivilege(L"SeRestorePrivilege");
    EnablePrivilege(L"SeTakeOwnershipPrivilege");
#endif
}

std::wstring SecurityContextMapper::LevelToStringSid(const std::wstring& levelStr) {
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

bool IntegrityLevelManager::SetIntegrityLevel(const fs::path& path, const std::wstring& stringSid, std::wostream& err) {
#ifdef _WIN32
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
#else
    (void)path;
    (void)stringSid;
    (void)err;
    return true;
#endif
}
