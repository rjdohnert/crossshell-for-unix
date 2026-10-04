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

#include "auditctl_app.hpp"
#include "audit_types.hpp"
#include "audit_policy.hpp"
#include "audit_session.hpp"
#include "cli.hpp"
#include <iostream>
#include <string>

int AuditctlApplication::run(int argc, wchar_t* argv[]) const {
    if (argc < 2) {
        PrintInvalidUsage(L"missing required option");
        return 1;
    }

    int argIndex = 1;
    bool dryRun = false;
    int outputFormat = 0;
    std::wstring pipeCommand;
    if (argIndex < argc) {
        std::wstring maybeDryRun = argv[argIndex];
        if (maybeDryRun == L"-n" || maybeDryRun == L"--dry-run") {
            dryRun = true;
            ++argIndex;
            if (argIndex >= argc) {
                PrintInvalidUsage(L"missing command after --dry-run");
                return 1;
            }
        }
    }

    while (argIndex < argc) {
        std::wstring option = argv[argIndex];
        if (option == L"--json") outputFormat = 1;
        else if (option == L"--csv") outputFormat = 2;
        else if (option == L"--table") outputFormat = 3;
        else if (option == L"--pipe" && argIndex + 1 < argc) pipeCommand = argv[++argIndex];
        else break;
        ++argIndex;
    }

    std::wstring arg1 = argv[argIndex];
    if (arg1 == L"-h" || arg1 == L"--help") {
        if (argc != argIndex + 1) {
            PrintInvalidUsage(L"--help does not accept extra arguments");
            return 1;
        }
        ShowHelp();
        return 0;
    }

    bool is_status = (arg1 == L"-s" || arg1 == L"--status");
    bool is_enable = (arg1 == L"-e" || arg1 == L"--enable");
    bool is_disable = (arg1 == L"-d" || arg1 == L"--disable");
    bool is_list = (arg1 == L"-l" || arg1 == L"--list-classes");

    if (!is_status && !is_enable && !is_disable && !is_list) {
        PrintInvalidUsage(L"unknown option");
        return 1;
    }

    AuditOutputSession outputSession(outputFormat, pipeCommand);

    std::wstring targetClass;
    if (is_status || is_list) {
        if (argc != argIndex + 1) {
            PrintInvalidUsage(is_list ? L"--list-classes does not accept a class argument" : L"--status does not accept a class argument");
            return 1;
        }
        if (is_list) {
            ShowClassList();
            return 0;
        }
    } else {
        if (argc < argIndex + 2) {
            PrintInvalidUsage(L"missing class; expected one of: proc, file, all");
            return 1;
        }
        if (argc > argIndex + 2) {
            PrintInvalidUsage(L"too many arguments");
            return 1;
        }
        targetClass = argv[argIndex + 1];
        if (!IsValidClass(targetClass)) {
            std::wcerr << L"auditctl: error: invalid class: " << targetClass
                      << L" (expected: proc/ex, file/fc, logon/logoff/lo, account/policy/ad, all)\n\n";
            ShowHelp();
            return 1;
        }
    }

    bool needsPrivilege = is_status || ((is_enable || is_disable) && !dryRun);
    if (needsPrivilege && !EnableSecurityPrivilege()) {
        std::wcerr << L"auditctl: error: Failed to enable SeSecurityPrivilege.\n"
                  << L"Make sure you are running as Administrator.\n";
        return 1;
    }

    if (is_status) {
        std::wcout << L"[Windows Audit Policy Status]\n";
        bool overall_ok = true;
        for (size_t i = 0; i < ClassCount(); ++i) {
            if (!QueryAuditSubcategory(GetAuditClass(i).statusLabel, GetAuditClass(i).guid)) {
                overall_ok = false;
            }
        }
        return overall_ok ? 0 : 1;
    }

    if (is_enable) {
        DWORD mask = POLICY_AUDIT_EVENT_SUCCESS | POLICY_AUDIT_EVENT_FAILURE;
        bool overall_ok = true;

        DWORD classMask = 0;
        ResolveTargetClassMask(targetClass, classMask);
        for (size_t i = 0; i < ClassCount(); ++i) {
            if ((classMask & (1u << i)) == 0) {
                continue;
            }

            if (dryRun) {
                std::wcout << L"auditctl: dry-run: would enable " << GetAuditClass(i).canonicalName
                           << L" (" << GetAuditClass(i).solarisAlias << L") with Success/Failure.\n";
                continue;
            }

            if (SetAuditSubcategory(GetAuditClass(i).guid, mask)) {
                std::wcout << GetAuditClass(i).enableMessage;
            } else {
                overall_ok = false;
                std::wcerr << L"auditctl: error: failed to enable " << GetAuditClass(i).canonicalName
                           << L" (Error " << GetLastError() << L")\n";
            }
        }

        return overall_ok ? 0 : 1;
    }

    if (is_disable) {
        DWORD mask = POLICY_AUDIT_EVENT_NONE;
        bool overall_ok = true;

        DWORD classMask = 0;
        ResolveTargetClassMask(targetClass, classMask);
        for (size_t i = 0; i < ClassCount(); ++i) {
            if ((classMask & (1u << i)) == 0) {
                continue;
            }

            if (dryRun) {
                std::wcout << L"auditctl: dry-run: would disable " << GetAuditClass(i).canonicalName
                           << L" (" << GetAuditClass(i).solarisAlias << L").\n";
                continue;
            }

            if (SetAuditSubcategory(GetAuditClass(i).guid, mask)) {
                std::wcout << GetAuditClass(i).disableMessage;
            } else {
                overall_ok = false;
                std::wcerr << L"auditctl: error: failed to disable " << GetAuditClass(i).canonicalName
                           << L" (Error " << GetLastError() << L")\n";
            }
        }

        return overall_ok ? 0 : 1;
    }

    PrintInvalidUsage(L"internal command dispatch failure");
    return 1;
}
