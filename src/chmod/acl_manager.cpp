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

#include "acl_manager.hpp"
#include "mode_parser.hpp"

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#pragma comment(lib, "Advapi32.lib")
#endif

namespace fs = std::filesystem;

bool AclPermissionManager::ApplyUnixPermissions(const fs::path& path, int uMode, int gMode, int oMode, std::wostream& err) {
#ifdef _WIN32
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
#else
    (void)path;
    (void)uMode;
    (void)gMode;
    (void)oMode;
    (void)err;
    return true;
#endif
}

bool AclPermissionManager::ProcessPath(const fs::path& target, const std::wstring& modeStr, std::wostream& err) {
    int userMode = 0, groupMode = 0, otherMode = 0;
    if (!ModeParser::Parse(modeStr, userMode, groupMode, otherMode, err)) {
        return false;
    }

    return ApplyUnixPermissions(target, userMode, groupMode, otherMode, err);
}
