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

#include "ownership.hpp"

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
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
    EnablePrivilege(L"SeChangeNotifyPrivilege");
    EnablePrivilege(L"SeTakeOwnershipPrivilege");
    EnablePrivilege(L"SeRestorePrivilege");
#endif
}

#ifdef _WIN32
ScopedSid SecurityAccountResolver::GetSidFromName(const std::wstring& name) {
    DWORD sidSize = 0;
    DWORD domainSize = 0;
    SID_NAME_USE sidUse;

    LookupAccountNameW(NULL, name.c_str(), NULL, &sidSize, NULL, &domainSize, &sidUse);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return ScopedSid(NULL);
    }

    PSID pSid = reinterpret_cast<PSID>(malloc(sidSize));
    if (!pSid) return ScopedSid(NULL);

    std::vector<wchar_t> domainBuffer(domainSize);

    if (!LookupAccountNameW(NULL, name.c_str(), pSid, &sidSize, domainBuffer.data(), &domainSize, &sidUse)) {
        free(pSid);
        return ScopedSid(NULL);
    }

    return ScopedSid(pSid);
}
#endif

void OwnershipSpecParser::Parse(const std::wstring& spec, std::wstring& outOwner, std::wstring& outGroup) {
    outOwner.clear();
    outGroup.clear();

    size_t sep = spec.find(L':');
    if (sep == std::wstring::npos) {
        sep = spec.find(L'.');
    }

    if (sep != std::wstring::npos) {
        outOwner = spec.substr(0, sep);
        outGroup = spec.substr(sep + 1);
    } else {
        outOwner = spec;
    }
}

bool OwnershipManager::ApplyOwnership(const fs::path& path, PSID pOwnerSid, PSID pGroupSid, std::wostream& err) {
#ifdef _WIN32
    SECURITY_INFORMATION secInfo = 0;

    if (pOwnerSid != NULL) {
        secInfo |= OWNER_SECURITY_INFORMATION;
    }
    if (pGroupSid != NULL) {
        secInfo |= GROUP_SECURITY_INFORMATION;
    }

    if (secInfo == 0) {
        return true;
    }

    std::wstring pathStr = path.wstring();

    DWORD dwRes = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(pathStr.c_str()),
        SE_FILE_OBJECT,
        secInfo,
        pOwnerSid,
        pGroupSid,
        NULL,
        NULL
    );

    if (dwRes != ERROR_SUCCESS) {
        err << L"Failed to set ownership for " << pathStr 
            << L" (Error Code: " << dwRes << L")\n";
        return false;
    }

    return true;
#else
    (void)path;
    (void)pOwnerSid;
    (void)pGroupSid;
    (void)err;
    return true;
#endif
}
