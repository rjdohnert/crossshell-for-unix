#include "engine.hpp"

// ----------------------------------------------------------------------------
// PrivilegeEscalator
// ----------------------------------------------------------------------------
bool PrivilegeEscalator::EnableDebugPrivilege() {
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

// ----------------------------------------------------------------------------
// ProcessKillerEngine
// ----------------------------------------------------------------------------
std::wstring ProcessKillerEngine::GetProcessOwner(DWORD pid) {
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

std::vector<ProcessEntry> ProcessKillerEngine::GetProcessList() {
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

BOOL CALLBACK ProcessKillerEngine::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
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

bool ProcessKillerEngine::SendWMClose(DWORD pid) {
    EnumData data = { pid, false };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.sent;
}

void ProcessKillerEngine::CollectTreePostOrder(DWORD pid, const std::vector<ProcessEntry>& allProcs, std::vector<DWORD>& orderedPids, std::set<DWORD>& visited) {
    if (visited.count(pid)) return;
    visited.insert(pid);

    for (const auto& proc : allProcs) {
        if (proc.parentPid == pid && proc.pid != pid && proc.pid != 0) {
            CollectTreePostOrder(proc.pid, allProcs, orderedPids, visited);
        }
    }
    orderedPids.push_back(pid);
}

bool ProcessKillerEngine::TerminateSingleProcess(const ProcessEntry& proc, const PskillOptions& config) {
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

int ProcessKillerEngine::Execute(const PskillOptions& config) {
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
