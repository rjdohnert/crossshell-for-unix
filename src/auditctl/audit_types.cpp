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

#include "audit_types.hpp"
#include <iostream>

// Process Creation: {0CCE922B-6929-4707-A510-09756F54AD09}
const GUID GUID_AUDIT_PROCESS_CREATION = 
    { 0x0CCE922B, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// File System Access: {0CCE921D-6929-4707-A510-09756F54AD09}
const GUID GUID_AUDIT_FILE_SYSTEM = 
    { 0x0CCE921D, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// Logon: {0CCE9215-6929-4707-A510-09756F54AD09}
const GUID GUID_AUDIT_LOGON = 
    { 0x0CCE9215, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// Logoff: {0CCE9216-6929-4707-A510-09756F54AD09}
const GUID GUID_AUDIT_LOGOFF = 
    { 0x0CCE9216, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// User Account Management: {0CCE920A-6929-4707-A510-09756F54AD09}
const GUID GUID_AUDIT_USER_ACCOUNT_MANAGEMENT = 
    { 0x0CCE920A, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// Audit Policy Change: {0CCE922F-6929-4707-A510-09756F54AD09}
const GUID GUID_AUDIT_POLICY_CHANGE = 
    { 0x0CCE922F, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

static const AuditClassDef kAuditClasses[] = {
    {
        L"proc",
        L"ex",
        L"Process Creation (ex)",
        L"auditctl: Process Creation auditing ENABLED (Success/Failure).\n",
        L"auditctl: Process Creation auditing DISABLED.\n",
        GUID_AUDIT_PROCESS_CREATION,
    },
    {
        L"file",
        L"fc",
        L"File System Access (fc)",
        L"auditctl: File System auditing ENABLED (Success/Failure).\n",
        L"auditctl: File System auditing DISABLED.\n",
        GUID_AUDIT_FILE_SYSTEM,
    },
    {
        L"logon",
        L"lo",
        L"Logon Auditing (lo)",
        L"auditctl: Logon auditing ENABLED (Success/Failure).\n",
        L"auditctl: Logon auditing DISABLED.\n",
        GUID_AUDIT_LOGON,
    },
    {
        L"logoff",
        L"lo",
        L"Logoff Auditing (lo)",
        L"auditctl: Logoff auditing ENABLED (Success/Failure).\n",
        L"auditctl: Logoff auditing DISABLED.\n",
        GUID_AUDIT_LOGOFF,
    },
    {
        L"account",
        L"ad",
        L"User Account Management (ad)",
        L"auditctl: User Account Management auditing ENABLED (Success/Failure).\n",
        L"auditctl: User Account Management auditing DISABLED.\n",
        GUID_AUDIT_USER_ACCOUNT_MANAGEMENT,
    },
    {
        L"policy",
        L"ad",
        L"Audit Policy Change (ad)",
        L"auditctl: Audit Policy Change auditing ENABLED (Success/Failure).\n",
        L"auditctl: Audit Policy Change auditing DISABLED.\n",
        GUID_AUDIT_POLICY_CHANGE,
    },
};

size_t ClassCount() {
    return sizeof(kAuditClasses) / sizeof(kAuditClasses[0]);
}

const AuditClassDef& GetAuditClass(size_t index) {
    return kAuditClasses[index];
}

bool ClassTokenMatches(const AuditClassDef& cls, const std::wstring& token) {
    return token == cls.canonicalName || token == cls.solarisAlias;
}

bool ResolveTargetClassMask(const std::wstring& token, DWORD& classMask) {
    if (token == L"all") {
        classMask = 0;
        for (size_t i = 0; i < ClassCount(); ++i) {
            classMask |= (1u << i);
        }
        return true;
    }

    classMask = 0;
    bool found = false;
    for (size_t i = 0; i < ClassCount(); ++i) {
        if (ClassTokenMatches(kAuditClasses[i], token)) {
            classMask |= (1u << i);
            found = true;
        }
    }

    return found;
}

bool IsValidClass(const std::wstring& targetClass) {
    DWORD mask = 0;
    return ResolveTargetClassMask(targetClass, mask);
}

void ShowClassList() {
    std::wcout << L"Supported classes:\n";
    for (size_t i = 0; i < ClassCount(); ++i) {
        std::wcout << L"  " << kAuditClasses[i].canonicalName
                   << L" (" << kAuditClasses[i].solarisAlias << L")\n";
    }
    std::wcout << L"  all\n";
}
