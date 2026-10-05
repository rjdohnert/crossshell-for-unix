// ============================================================================
// Process Signaler: Action implementation
// ============================================================================
class ProcessSignaler {
private:
    PFN_NtSuspendProcess NtSuspendProcess = nullptr;
    PFN_NtResumeProcess NtResumeProcess = nullptr;

    struct EnumWndData {
        DWORD targetPid;
        std::vector<HWND> windows;
    };

    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
        auto* data = reinterpret_cast<EnumWndData*>(lParam);
        DWORD procId = 0;
        ::GetWindowThreadProcessId(hwnd, &procId);
        if (procId == data->targetPid && ::IsWindowVisible(hwnd)) {
            data->windows.push_back(hwnd);
        }
        return TRUE;
    }

    bool sendGracefulClose(DWORD pid, DWORD timeoutMs = 2000) const {
        EnumWndData data{ pid, {} };
        ::EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));

        if (data.windows.empty()) {
            return false;
        }

        for (HWND hwnd : data.windows) {
            ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }

        HANDLE hProc = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (hProc) {
            UniqueHandle procHandle(hProc);
            DWORD waitRes = ::WaitForSingleObject(procHandle.get(), timeoutMs);
            return (waitRes == WAIT_OBJECT_0);
        }
        return false;
    }

public:
    ProcessSignaler() {
        HMODULE hNtDll = ::GetModuleHandleW(L"ntdll.dll");
        if (hNtDll) {
            NtSuspendProcess = reinterpret_cast<PFN_NtSuspendProcess>(::GetProcAddress(hNtDll, "NtSuspendProcess"));
            NtResumeProcess = reinterpret_cast<PFN_NtResumeProcess>(::GetProcAddress(hNtDll, "NtResumeProcess"));
        }
    }

    bool dispatchSignal(DWORD pid, const SignalInfo& sig, bool force) const {
        // Signal 0: Process probe / existence check
        if (sig.type == SignalType::SIGNULL) {
            HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            DWORD exitCode = 0;
            return ::GetExitCodeProcess(handle.get(), &exitCode) && (exitCode == STILL_ACTIVE);
        }

        // SIGSTOP: Suspend process execution
        if (sig.type == SignalType::SIGSTOP) {
            if (!NtSuspendProcess) return false;
            HANDLE hProc = ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            return (NtSuspendProcess(handle.get()) >= 0);
        }

        // SIGCONT: Resume process execution
        if (sig.type == SignalType::SIGCONT) {
            if (!NtResumeProcess) return false;
            HANDLE hProc = ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            return (NtResumeProcess(handle.get()) >= 0);
        }

        // SIGINT / SIGQUIT: Console control signals
        if (sig.type == SignalType::SIGINT || sig.type == SignalType::SIGQUIT) {
            DWORD ctrlEvent = (sig.type == SignalType::SIGINT) ? CTRL_C_EVENT : CTRL_BREAK_EVENT;
            if (::GenerateConsoleCtrlEvent(ctrlEvent, pid)) {
                return true;
            }
        }

        // SIGTERM / SIGHUP: Graceful termination attempt
        if ((sig.type == SignalType::SIGTERM || sig.type == SignalType::SIGHUP) && !force) {
            if (sendGracefulClose(pid)) {
                return true;
            }
        }

        // SIGKILL or fallback for unhandled/forced termination
        HANDLE hProc = ::OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!hProc) return false;
        UniqueHandle handle(hProc);

        UINT exitCode = (sig.number != 0) ? (128 + sig.number) : 1;
        return ::TerminateProcess(handle.get(), exitCode) != FALSE;
    }
};

// ============================================================================
