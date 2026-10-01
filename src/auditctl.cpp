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
/*
 * auditctl.cpp - Audit Control Utility
 * Copyright (C) 2026, Roberto J Dohnert
 */

#include <windows.h>
#include <ntsecapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>

#pragma comment(lib, "Advapi32.lib")

// Well-known Audit Subcategory GUIDs (defined in winnt.h / ntsecapi.h)
// Process Creation: {0CCE922B-6929-4707-A510-09756F54AD09}
static const GUID GUID_AUDIT_PROCESS_CREATION = 
    { 0x0CCE922B, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// File System Access: {0CCE921D-6929-4707-A510-09756F54AD09}
static const GUID GUID_AUDIT_FILE_SYSTEM = 
    { 0x0CCE921D, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// Logon: {0CCE9215-6929-4707-A510-09756F54AD09}
static const GUID GUID_AUDIT_LOGON = 
    { 0x0CCE9215, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// Logoff: {0CCE9216-6929-4707-A510-09756F54AD09}
static const GUID GUID_AUDIT_LOGOFF = 
    { 0x0CCE9216, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// User Account Management: {0CCE920A-6929-4707-A510-09756F54AD09}
static const GUID GUID_AUDIT_USER_ACCOUNT_MANAGEMENT = 
    { 0x0CCE920A, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

// Audit Policy Change: {0CCE922F-6929-4707-A510-09756F54AD09}
static const GUID GUID_AUDIT_POLICY_CHANGE = 
    { 0x0CCE922F, 0x6929, 0x4707, { 0xA5, 0x10, 0x09, 0x75, 0x6F, 0x54, 0xAD, 0x09 } };

void ShowHelp();

class AuditOutputSession {
    int format_;
    std::wstring pipe_;
    std::wstreambuf* old_;
    std::wostringstream capture_;
    static std::string narrow(const std::wstring& value) { std::string result; for (wchar_t ch : value) result.push_back(static_cast<char>(ch)); return result; }
public:
    AuditOutputSession(int format, std::wstring pipe) : format_(format), pipe_(std::move(pipe)), old_(nullptr) {
        if (format_ || !pipe_.empty()) { old_ = std::wcout.rdbuf(capture_.rdbuf()); }
    }
    ~AuditOutputSession() {
        if (!old_) return;
        std::wcout.rdbuf(old_);
        std::wstring data = capture_.str();
        std::wstring output;
        if (format_ == 1) output = L"[{\"output\":\"" + data + L"\"}]\n";
        else if (format_ == 2) output = L"\"output\"\n\"" + data + L"\"\n";
        else output = L"OUTPUT\n------\n" + data;
        if (!pipe_.empty()) { FILE* pipe = _wpopen(pipe_.c_str(), L"w"); if (pipe) { std::string text = narrow(output); std::fwrite(text.data(), 1, text.size(), pipe); _pclose(pipe); } }
        else std::wcout << output;
    }
};

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

struct AuditClassDef {
    const wchar_t* canonicalName;
    const wchar_t* solarisAlias;
    const wchar_t* statusLabel;
    const wchar_t* enableMessage;
    const wchar_t* disableMessage;
    GUID guid;
};

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

void PrintInvalidUsage(const wchar_t* message) {
    std::wcerr << L"auditctl: error: " << message << L"\n\n";
    ShowHelp();
}

void ShowHelp() {
    std::wcout << LR"(auditctl(1)             CrossShell for UNIX Reference Manual            auditctl(1)

    NAME
        auditctl - Display and update Windows Advanced Audit Policy.

    SYNOPSIS
        auditctl [--json | --csv | --table] [--pipe COMMAND] --status
        auditctl [--json | --csv | --table] [--pipe COMMAND] --list-classes
        auditctl [--dry-run] [--json | --csv | --table] [--pipe COMMAND]
                 (--enable | --disable) CLASS
        auditctl --help

    DESCRIPTION
        Displays and updates selected Windows Advanced Audit Policy
        subcategories using Solaris-style class names. Policy data is read
        and changed through the Windows Audit Policy APIs. Output uses the
        Windows console and standard streams and can be formatted or piped
        to another command.

    OPTIONS
        -s, --status
            Show the current audit state for all supported classes.

        -e, --enable CLASS
            Enable Success and Failure auditing for CLASS.

        -d, --disable CLASS
            Disable auditing for CLASS.

        -l, --list-classes
            List supported class names and Solaris-style aliases.

        -n, --dry-run
            Print intended enable or disable changes without applying them.

        --json
            Format command output as JSON.

        --csv
            Format command output as CSV.

        --table
            Format command output as a table.

        --pipe COMMAND
            Send formatted output through COMMAND.

        -h, --help
            Display this reference manual.

    AVAILABLE CLASSES
        proc, ex
            Process Creation and Execution events.

        file, fc
            File System object access events.

        logon, logoff, lo
            Logon and logoff events. The lo alias selects both classes.

        account, policy, ad
            Account management and audit policy changes. The ad alias
            selects both classes.

        all
            Select every supported class.

    NOTES
        Policy changes require Administrator rights and SeSecurityPrivilege.
        Status queries also require elevation. Enabling a class selects both
        Success and Failure auditing; disabling selects No Auditing.

    EXAMPLES
        auditctl --status
            Display the current state of every supported audit class.

        auditctl --enable proc
            Enable Success and Failure auditing for process creation.

        auditctl --dry-run --disable all
            Preview disabling every supported audit class.

        auditctl --json --status
            Display audit status as JSON.

        auditctl --csv --pipe "more" --status
            Send CSV-formatted audit status through another command.

    CrossShell for UNIX                                                     auditctl(1)
    )";
}

static int auditctl_main(int argc, wchar_t* argv[]) {
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
            if (!QueryAuditSubcategory(kAuditClasses[i].statusLabel, kAuditClasses[i].guid)) {
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
                std::wcout << L"auditctl: dry-run: would enable " << kAuditClasses[i].canonicalName
                           << L" (" << kAuditClasses[i].solarisAlias << L") with Success/Failure.\n";
                continue;
            }

            if (SetAuditSubcategory(kAuditClasses[i].guid, mask)) {
                std::wcout << kAuditClasses[i].enableMessage;
            } else {
                overall_ok = false;
                std::wcerr << L"auditctl: error: failed to enable " << kAuditClasses[i].canonicalName
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
                std::wcout << L"auditctl: dry-run: would disable " << kAuditClasses[i].canonicalName
                           << L" (" << kAuditClasses[i].solarisAlias << L").\n";
                continue;
            }

            if (SetAuditSubcategory(kAuditClasses[i].guid, mask)) {
                std::wcout << kAuditClasses[i].disableMessage;
            } else {
                overall_ok = false;
                std::wcerr << L"auditctl: error: failed to disable " << kAuditClasses[i].canonicalName
                           << L" (Error " << GetLastError() << L")\n";
            }
        }

        return overall_ok ? 0 : 1;
    }

    PrintInvalidUsage(L"internal command dispatch failure");
    return 1;
}

class AuditctlApplication {
public:
    int run(int argc, wchar_t* argv[]) const { return auditctl_main(argc, argv); }
};

int wmain(int argc, wchar_t* argv[]) {
    return AuditctlApplication().run(argc, argv);
}
