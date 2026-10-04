/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *
 * CrossShell for UNIX
 */

#include "audit_policy.hpp"
#include <iostream>

#pragma comment(lib, "Advapi32.lib")

// Enable SeSecurityPrivilege required for modifying Windows Audit Policy
bool EnableSecurityPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValueW(NULL, L"SeSecurityPrivilege", &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    CloseHandle(hToken);
    return (result && GetLastError() == ERROR_SUCCESS);
}

// Display status of an Audit Subcategory
bool QueryAuditSubcategory(const wchar_t* name, const GUID& subCategoryGuid) {
    PAUDIT_POLICY_INFORMATION pPolicy = NULL;

    if (AuditQuerySystemPolicy(&subCategoryGuid, 1, &pPolicy) && pPolicy) {
        ULONG mask = pPolicy->AuditingInformation;
        std::wcout << L"  " << name << L": ";

        if (mask == POLICY_AUDIT_EVENT_NONE) {
            std::wcout << L"No Auditing (Disabled)\n";
        } else {
            if (mask & POLICY_AUDIT_EVENT_SUCCESS) std::wcout << L"[Success] ";
            if (mask & POLICY_AUDIT_EVENT_FAILURE) std::wcout << L"[Failure] ";
            std::wcout << L"\n";
        }
        AuditFree(pPolicy);
        return true;
    } else {
        std::wcerr << L"  " << name << L": Failed to query status (Error " << GetLastError() << L")\n";
        return false;
    }
}

// Set Audit Policy for a Subcategory (Solaris auditctl -s / -m equivalent)
bool SetAuditSubcategory(const GUID& subCategoryGuid, DWORD auditFlags) {
    AUDIT_POLICY_INFORMATION policy = {};
    policy.AuditSubCategoryGuid = subCategoryGuid;
    policy.AuditingInformation = auditFlags;

    if (AuditSetSystemPolicy(&policy, 1)) {
        return true;
    }
    return false;
}
