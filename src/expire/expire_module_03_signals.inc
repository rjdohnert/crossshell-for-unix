void terminate_process_tree_snapshot(DWORD root_pid, UINT exit_code) {
    ScopedHandle hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnap.is_valid()) return;

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    std::vector<DWORD> pids_to_kill;

    if (Process32FirstW(hSnap.get(), &pe)) {
        do {
            if (pe.th32ParentProcessID == root_pid) {
                pids_to_kill.push_back(pe.th32ProcessID);
            }
        } while (Process32NextW(hSnap.get(), &pe));
    }

    // Recursively kill child process trees first
    for (DWORD child_pid : pids_to_kill) {
        terminate_process_tree_snapshot(child_pid, exit_code);
    }

    // Terminate root PID
    ScopedHandle hProc(OpenProcess(PROCESS_TERMINATE, FALSE, root_pid));
    if (hProc.is_valid()) {
        TerminateProcess(hProc.get(), exit_code);
    }
}

//
// SIGNAL EMULATION & PROCESS TERMINATION
// 

BOOL CALLBACK kill_window_enum_proc(HWND hWnd, LPARAM lParam) {
    DWORD target_pid = static_cast<DWORD>(lParam);
    DWORD win_pid = 0;
    GetWindowThreadProcessId(hWnd, &win_pid);
    if (win_pid == target_pid && IsWindowVisible(hWnd)) {
        PostMessageW(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

void dispatch_win32_signal(HANDLE hJob, HANDLE hProcess, DWORD pid, DWORD signal, bool verbose) {
    if (verbose) {
        std::wcerr << L"expire: sending signal " << get_signal_name(signal)
                   << L" (" << signal << L") to command PID " << pid << L"\n";
    }

    if (signal == SIG_KILL) {
        if (hJob != INVALID_HANDLE_VALUE && hJob != NULL) {
            TerminateJobObject(hJob, EXIT_FORCE_KILLED);
        } else {
            terminate_process_tree_snapshot(pid, EXIT_FORCE_KILLED);
        }
        return;
    }

    if (signal == SIG_INT || signal == SIG_QUIT) {
        // CTRL_BREAK_EVENT reliably reaches process groups created with CREATE_NEW_PROCESS_GROUP
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
        return;
    }

    if (signal == SIG_TERM || signal == SIG_HUP) {
        // Attempt graceful window close for GUI processes
        EnumWindows(kill_window_enum_proc, static_cast<LPARAM>(pid));
        
        // Send CTRL_BREAK as a soft interrupt for console processes
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);

        // Fallback: Terminate process tree if not terminated within 250ms
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        DWORD code = 0;
        if (GetExitCodeProcess(hProcess, &code) && code == STILL_ACTIVE) {
            if (hJob != INVALID_HANDLE_VALUE && hJob != NULL) {
                TerminateJobObject(hJob, EXIT_TIMEOUT_EXPIRED);
            } else {
                terminate_process_tree_snapshot(pid, EXIT_TIMEOUT_EXPIRED);
            }
        }
    }
}

