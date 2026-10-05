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
#include <tlhelp32.h>
#include <aclapi.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <cwctype>
#include <iomanip>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// 1. CONSTANTS, ENUMS & DATA MODELS
// ============================================================================

constexpr wchar_t VERSION[] = L"2.2.0";

enum ExitCode {
    EXIT_SUCCESS_OK      = 0,
    EXIT_PARTIAL_FAILURE = 1,
    EXIT_INVALID_ARGS    = 2,
    EXIT_NO_MATCH        = 3,
    EXIT_PRIVILEGE_ERR   = 4
};

enum SignalType {
    SIG_TERM = 15,
    SIG_KILL = 9,
    SIG_INT  = 2
};

struct ProcessEntry {
    DWORD pid = 0;
    DWORD parentPid = 0;
    std::wstring name;
    std::wstring owner;
};

// ============================================================================
// 2. RAII HANDLES & PRIVILEGE MANAGEMENT
// ============================================================================

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : m_h(h) {}
    ~ScopedHandle() { Close(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& o) noexcept : m_h(o.m_h) { o.m_h = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& o) noexcept {
        if (this != &o) {
            Close();
            m_h = o.m_h;
            o.m_h = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_h; }
    bool IsValid() const { return m_h != NULL && m_h != INVALID_HANDLE_VALUE; }
    operator HANDLE() const { return m_h; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_h);
            m_h = INVALID_HANDLE_VALUE;
        }
    }

    void Reset(HANDLE h = INVALID_HANDLE_VALUE) {
        Close();
        m_h = h;
    }

private:
    HANDLE m_h;
};

class PrivilegeEscalator {
public:
    static bool EnableDebugPrivilege() {
        HANDLE hTokenRaw = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTokenRaw)) {
            return false;
        }
        ScopedHandle hToken(hTokenRaw);

        TOKEN_PRIVILEGES tp = {};
        LUID luid = {};
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        SetLastError(ERROR_SUCCESS);
        if (!AdjustTokenPrivileges(hToken.Get(), FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
            return false;
        }

        return (GetLastError() == ERROR_SUCCESS);
    }
};

// ============================================================================
// 3. OPTIONS PARSER & STRING UTILS
// ============================================================================

class PskillOptions {
public:
    std::vector<std::wstring> targets;
    SignalType signal = SIG_TERM;
    bool force = false;
    bool tree = false;
    bool exact = false;
    bool caseSensitive = false;
    bool dryRun = false;
    bool verbose = false;
    bool quiet = false;
    std::wstring userFilter;
    bool showHelp = false;

    static std::wstring ToLower(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), ::towlower);
        return str;
    }

    static bool TryParsePID(const std::wstring& str, DWORD& pid) {
        if (str.empty()) return false;
        for (wchar_t c : str) {
            if (!std::iswdigit(c)) return false;
        }
        try {
            pid = std::stoul(str);
            return true;
        } catch (...) {
            return false;
        }
    }

    int Parse(int argc, wchar_t* argv[]) {
        if (argc < 2) {
            showHelp = true;
            return EXIT_INVALID_ARGS;
        }

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            std::wstring key = arg;
            std::wstring val = L"";

            size_t eqPos = arg.find(L'=');
            if (eqPos != std::wstring::npos) {
                key = arg.substr(0, eqPos);
                val = arg.substr(eqPos + 1);
            }

            if (key == L"-h" || key == L"--help" || key == L"/?") {
                showHelp = true;
                return EXIT_SUCCESS_OK;
            } else if (key == L"-f" || key == L"--force") {
                force = true;
                signal = SIG_KILL;
            } else if (key == L"-t" || key == L"--tree") {
                tree = true;
            } else if (key == L"-e" || key == L"--exact") {
                exact = true;
            } else if (key == L"-c" || key == L"--case-sensitive") {
                caseSensitive = true;
            } else if (key == L"-n" || key == L"--dry-run") {
                dryRun = true;
            } else if (key == L"-v" || key == L"--verbose") {
                verbose = true;
            } else if (key == L"-q" || key == L"--quiet") {
                quiet = true;
            } else if (key == L"-u" || key == L"--user") {
                if (!val.empty()) {
                    userFilter = val;
                } else if (i + 1 < argc) {
                    userFilter = argv[++i];
                } else {
                    std::wcerr << L"Error: Option " << key << L" requires a user argument.\n";
                    return EXIT_INVALID_ARGS;
                }
            } else if (key == L"-s" || key == L"--signal") {
                std::wstring sigStr = val;
                if (sigStr.empty() && i + 1 < argc) {
                    sigStr = argv[++i];
                }
                if (sigStr == L"9" || sigStr == L"KILL" || sigStr == L"SIGKILL") {
                    signal = SIG_KILL;
                } else if (sigStr == L"15" || sigStr == L"TERM" || sigStr == L"SIGTERM") {
                    signal = SIG_TERM;
                } else if (sigStr == L"2" || sigStr == L"INT" || sigStr == L"SIGINT") {
                    signal = SIG_INT;
                } else {
                    std::wcerr << L"Error: Unsupported signal identifier: " << sigStr << L"\n";
                    return EXIT_INVALID_ARGS;
                }
            } else if (key.rfind(L"-", 0) == 0) {
                std::wcerr << L"Error: Unrecognized option '" << key << L"'. Run 'pskill --help' for usage.\n";
                return EXIT_INVALID_ARGS;
            } else {
                targets.push_back(arg);
            }
        }

        if (targets.empty()) {
            std::wcerr << L"Error: No target processes specified.\n";
            return EXIT_INVALID_ARGS;
        }

        return -1;
    }

    void PrintHelp() const {
        std::wcout << LR"(pskill(1)               CrossShell for UNIX Reference Manual                pskill(1)

    NAME
        pskill - kill processes by name, PID, or remote computer

    SYNOPSIS
        pskill [OPTIONS] [\\COMPUTER] [PID|PROCESS_NAME]

    DESCRIPTION
        pskill terminates processes running on the local Windows system or on a
        remote Windows computer using standard Win32 / WMI management APIs.

    OPTIONS
        -t, --tree
            Kill the process and all of its descendant child processes.

        -u USER, --user USER
            Target only processes running under the specified username.

        --json, --csv, --table
            Format termination status as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        pskill -t 1234
            Kill process 1234 and its complete process sub-tree.

        pskill calc.exe
            Kill all running Calculator instances.

    CrossShell for UNIX                                                 pskill(1)
)";
    }
};

// ============================================================================
// 4. PROCESS KILLER ENGINE
// ============================================================================

class ProcessKillerEngine {
public:
    static std::wstring GetProcessOwner(DWORD pid) {
        ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (!hProcess.IsValid()) return L"N/A";

        HANDLE hTokenRaw = nullptr;
        if (!OpenProcessToken(hProcess.Get(), TOKEN_QUERY, &hTokenRaw)) return L"N/A";
        ScopedHandle hToken(hTokenRaw);

        DWORD dwSize = 0;
        GetTokenInformation(hToken.Get(), TokenUser, NULL, 0, &dwSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return L"N/A";

        std::vector<BYTE> buffer(dwSize);
        PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
        if (!GetTokenInformation(hToken.Get(), TokenUser, pTokenUser, dwSize, &dwSize)) return L"N/A";

        WCHAR name[256], domain[256];
        DWORD dwName = 256, dwDomain = 256;
        SID_NAME_USE snu;

        if (LookupAccountSidW(NULL, pTokenUser->User.Sid, name, &dwName, domain, &dwDomain, &snu)) {
            return std::wstring(domain) + L"\\" + name;
        }

        return L"N/A";
    }

    static std::vector<ProcessEntry> GetProcessList() {
        std::vector<ProcessEntry> list;
        ScopedHandle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!hSnapshot.IsValid()) return list;

        PROCESSENTRY32W pe32 = {};
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot.Get(), &pe32)) {
            do {
                ProcessEntry entry;
                entry.pid = pe32.th32ProcessID;
                entry.parentPid = pe32.th32ParentProcessID;
                entry.name = pe32.szExeFile;
                list.push_back(entry);
            } while (Process32NextW(hSnapshot.Get(), &pe32));
        }

        return list;
    }

    struct EnumData { DWORD pid; bool sent; };
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        EnumData* data = reinterpret_cast<EnumData*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == data->pid) {
            if (IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) == NULL) {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                data->sent = true;
            }
        }
        return TRUE;
    }

    static bool SendWMClose(DWORD pid) {
        EnumData data = { pid, false };
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
        return data.sent;
    }

    static void CollectTreePostOrder(DWORD pid, const std::vector<ProcessEntry>& allProcs, std::vector<DWORD>& orderedPids, std::set<DWORD>& visited) {
        if (visited.count(pid)) return;
        visited.insert(pid);

        for (const auto& proc : allProcs) {
            if (proc.parentPid == pid && proc.pid != pid && proc.pid != 0) {
                CollectTreePostOrder(proc.pid, allProcs, orderedPids, visited);
            }
        }
        orderedPids.push_back(pid);
    }

    static bool TerminateSingleProcess(const ProcessEntry& proc, const PskillOptions& config) {
        if (proc.pid == 0 || proc.pid == 4) {
            if (!config.quiet) std::wcout << L"[SKIP] Cannot terminate System/Idle process (PID " << proc.pid << L")\n";
            return false;
        }

        if (proc.pid == GetCurrentProcessId()) {
            if (!config.quiet) std::wcout << L"[SKIP] Will not terminate self (PID " << proc.pid << L")\n";
            return false;
        }

        if (config.dryRun) {
            std::wcout << L"[DRY-RUN] Would kill PID " << proc.pid << L" (" << proc.name << L")\n";
            return true;
        }

        if (config.signal == SIG_INT || (config.signal == SIG_TERM && !config.force)) {
            bool windowFound = SendWMClose(proc.pid);
            if (windowFound) {
                if (config.verbose && !config.quiet) {
                    std::wcout << L"[INFO] Sent WM_CLOSE to PID " << proc.pid << L" (" << proc.name << L")\n";
                }

                ScopedHandle hProcWait(OpenProcess(SYNCHRONIZE, FALSE, proc.pid));
                if (hProcWait.IsValid()) {
                    DWORD waitResult = WaitForSingleObject(hProcWait.Get(), 1500);
                    if (waitResult == WAIT_OBJECT_0) {
                        if (!config.quiet) {
                            std::wcout << L"[SUCCESS] Process PID " << proc.pid << L" (" << proc.name << L") exited gracefully.\n";
                        }
                        return true;
                    }
                }
            }

            if (config.signal == SIG_INT) {
                if (!windowFound && !config.quiet) {
                    std::wcout << L"[SKIP] No window found to receive SIGINT on PID " << proc.pid << L"\n";
                }
                return windowFound;
            }
        }

        ScopedHandle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, proc.pid));
        if (!hProcess.IsValid()) {
            if (!config.quiet) {
                std::wcerr << L"[ERROR] Access denied or process vanished for PID " << proc.pid 
                           << L" (" << proc.name << L") - Error Code: " << GetLastError() << L"\n";
            }
            return false;
        }

        if (!TerminateProcess(hProcess.Get(), 1)) {
            if (!config.quiet) {
                std::wcerr << L"[ERROR] Failed to terminate PID " << proc.pid 
                           << L" (" << proc.name << L") - Error Code: " << GetLastError() << L"\n";
            }
            return false;
        }

        if (!config.quiet) {
            std::wcout << L"[SUCCESS] Terminated PID " << proc.pid << L" (" << proc.name << L")\n";
        }

        return true;
    }

    static int Execute(const PskillOptions& config) {
        if (!PrivilegeEscalator::EnableDebugPrivilege() && config.verbose && !config.quiet) {
            std::wcout << L"[WARNING] Could not obtain SeDebugPrivilege. Lower privilege access limits apply.\n";
        }

        std::vector<ProcessEntry> allProcesses = GetProcessList();
        if (allProcesses.empty()) {
            std::wcerr << L"Error: Failed to enumerate running processes (Snapshot Error).\n";
            return EXIT_PARTIAL_FAILURE;
        }

        std::set<DWORD> initialTargetPids;

        for (const auto& target : config.targets) {
            DWORD targetPid = 0;

            if (PskillOptions::TryParsePID(target, targetPid)) {
                initialTargetPids.insert(targetPid);
            } else {
                std::wstring needle = config.caseSensitive ? target : PskillOptions::ToLower(target);
                std::wstring needleWithExe = (needle.rfind(L".exe") == std::wstring::npos) ? needle + L".exe" : needle;

                for (auto& proc : allProcesses) {
                    std::wstring haystack = config.caseSensitive ? proc.name : PskillOptions::ToLower(proc.name);

                    bool match = false;
                    if (config.exact) {
                        match = (haystack == needle || haystack == needleWithExe);
                    } else {
                        match = (haystack.find(needle) != std::wstring::npos);
                    }

                    if (match) {
                        initialTargetPids.insert(proc.pid);
                    }
                }
            }
        }

        if (initialTargetPids.empty()) {
            if (!config.quiet) std::wcout << L"No running processes matched the specified criteria.\n";
            return EXIT_NO_MATCH;
        }

        std::vector<DWORD> finalOrderedPids;
        std::set<DWORD> visitedPids;

        for (DWORD pid : initialTargetPids) {
            if (config.tree) {
                CollectTreePostOrder(pid, allProcesses, finalOrderedPids, visitedPids);
            } else {
                if (visitedPids.find(pid) == visitedPids.end()) {
                    visitedPids.insert(pid);
                    finalOrderedPids.push_back(pid);
                }
            }
        }

        if (!config.userFilter.empty()) {
            std::vector<DWORD> userFilteredPids;
            std::wstring targetUserLower = PskillOptions::ToLower(config.userFilter);

            for (DWORD pid : finalOrderedPids) {
                std::wstring owner = GetProcessOwner(pid);
                if (PskillOptions::ToLower(owner).find(targetUserLower) != std::wstring::npos) {
                    userFilteredPids.push_back(pid);
                }
            }
            finalOrderedPids = userFilteredPids;
        }

        if (finalOrderedPids.empty()) {
            if (!config.quiet) std::wcout << L"No running processes matched the user filter criteria.\n";
            return EXIT_NO_MATCH;
        }

        int successCount = 0;
        int failCount = 0;

        for (DWORD pid : finalOrderedPids) {
            ProcessEntry procMeta = { pid, 0, L"Unknown", L"" };
            for (const auto& p : allProcesses) {
                if (p.pid == pid) {
                    procMeta = p;
                    break;
                }
            }

            if (config.verbose && !config.quiet) {
                procMeta.owner = GetProcessOwner(pid);
                std::wcout << L"[TARGET] PID: " << std::setw(6) << procMeta.pid 
                           << L" | Name: " << std::setw(20) << procMeta.name 
                           << L" | Owner: " << procMeta.owner << L"\n";
            }

            if (TerminateSingleProcess(procMeta, config)) {
                successCount++;
            } else {
                failCount++;
            }
        }

        if (failCount > 0 && successCount == 0) return EXIT_PARTIAL_FAILURE;
        if (failCount > 0) return EXIT_PARTIAL_FAILURE;

        return EXIT_SUCCESS_OK;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class PskillApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        PskillOptions options;
        int parseResult = options.Parse(argc, argv);
        if (parseResult >= 0) {
            if (options.showHelp) {
                options.PrintHelp();
                return (argc < 2) ? EXIT_INVALID_ARGS : EXIT_SUCCESS_OK;
            }
            return parseResult;
        }

        return ProcessKillerEngine::Execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    PskillApplication app;
    return app.Run(argc, argv);
}