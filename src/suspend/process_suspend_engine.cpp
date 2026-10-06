#include "process_suspend_engine.hpp"

bool ProcessSuspendEngine::OperateWithNtdll(HANDLE process_handle, bool resume) {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll == nullptr) {
            return false;
        }

        auto suspend_fn = reinterpret_cast<NtSuspendProcessFn>(GetProcAddress(ntdll, "NtSuspendProcess"));
        auto resume_fn = reinterpret_cast<NtResumeProcessFn>(GetProcAddress(ntdll, "NtResumeProcess"));
        if (!resume && suspend_fn == nullptr) {
            return false;
        }
        if (resume && resume_fn == nullptr) {
            return false;
        }

        LONG status = resume ? resume_fn(process_handle) : suspend_fn(process_handle);
        return status >= 0;
    }

bool ProcessSuspendEngine::OperateWithThreads(DWORD pid, bool resume) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        bool touched = false;

        if (Thread32First(snapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID != pid) {
                    continue;
                }

                HANDLE thread_handle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
                if (thread_handle == nullptr) {
                    continue;
                }

                if (resume) {
                    while (ResumeThread(thread_handle) > 0) {
                    }
                } else {
                    SuspendThread(thread_handle);
                }

                touched = true;
                CloseHandle(thread_handle);
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return touched;
    }

bool ProcessSuspendEngine::OperateOnPid(DWORD pid, bool resume, bool use_ntdll) {
        if (pid == GetCurrentProcessId()) {
            std::wcerr << L"suspend: refusing to target the current process\n";
            return false;
        }

        HANDLE process_handle = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process_handle != nullptr) {
            bool ok = false;
            if (use_ntdll) {
                ok = OperateWithNtdll(process_handle, resume);
            }
            CloseHandle(process_handle);
            if (ok) {
                return true;
            }
        }

        return OperateWithThreads(pid, resume);
    }
