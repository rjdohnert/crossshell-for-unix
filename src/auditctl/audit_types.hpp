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

#ifndef AUDITCTL_TYPES_HPP
#define AUDITCTL_TYPES_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Well-known Audit Subcategory GUIDs (defined in winnt.h / ntsecapi.h)
extern const GUID GUID_AUDIT_PROCESS_CREATION;
extern const GUID GUID_AUDIT_FILE_SYSTEM;
extern const GUID GUID_AUDIT_LOGON;
extern const GUID GUID_AUDIT_LOGOFF;
extern const GUID GUID_AUDIT_USER_ACCOUNT_MANAGEMENT;
extern const GUID GUID_AUDIT_POLICY_CHANGE;

struct AuditClassDef {
    const wchar_t* canonicalName;
    const wchar_t* solarisAlias;
    const wchar_t* statusLabel;
    const wchar_t* enableMessage;
    const wchar_t* disableMessage;
    GUID guid;
};

size_t ClassCount();
const AuditClassDef& GetAuditClass(size_t index);
bool ClassTokenMatches(const AuditClassDef& cls, const std::wstring& token);
bool ResolveTargetClassMask(const std::wstring& token, DWORD& classMask);
bool IsValidClass(const std::wstring& targetClass);
void ShowClassList();

#endif // AUDITCTL_TYPES_HPP
