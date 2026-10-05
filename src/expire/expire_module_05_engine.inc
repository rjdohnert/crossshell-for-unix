static int expire_main(int argc, wchar_t* argv[]) {
    g_expire_format = 0;
    g_expire_pipe.clear();
    if (argc <= 1) {
        std::wcerr << L"expire: missing operand\n"
                   << L"Try 'expire --help' for more information.\n";
        return EXIT_INTERNAL_ERROR;
    }

    DWORD initial_signal = SIG_TERM;
    double timeout_seconds = 0.0;
    double kill_after_seconds = -1.0;
    bool preserve_status = false;
    bool foreground = false;
    bool verbose = false;

    int cmd_idx = 1;
    bool stop_flags = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (!stop_flags && arg == L"--") {
            stop_flags = true;
            cmd_idx = i + 1;
            continue;
        }

        if (!stop_flags && !arg.empty() && arg[0] == L'-' && arg.length() > 1) {
            if (arg == L"--help") {
                print_help();
                return 0;
            }
            if (arg == L"--version") {
                print_version();
                return 0;
            }
            if (arg == L"--json") { g_expire_format = 1; continue; }
            if (arg == L"--csv") { g_expire_format = 2; continue; }
            if (arg == L"--table") { g_expire_format = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { g_expire_pipe = argv[++i]; continue; }
            if (arg == L"--preserve-status") {
                preserve_status = true;
                continue;
            }
            if (arg == L"-f" || arg == L"--foreground") {
                foreground = true;
                continue;
            }
            if (arg == L"-v" || arg == L"--verbose") {
                verbose = true;
                continue;
            }
            if (arg.rfind(L"-s=", 0) == 0 || arg.rfind(L"--signal=", 0) == 0) {
                size_t eq = arg.find(L'=');
                if (!parse_signal(arg.substr(eq + 1), initial_signal)) {
                    std::wcerr << L"expire: invalid signal: " << arg.substr(eq + 1) << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }
            if (arg == L"-s" || arg == L"--signal") {
                if (i + 1 >= argc) {
                    std::wcerr << L"expire: option requires an argument -- '" << arg << L"'\n";
                    return EXIT_INTERNAL_ERROR;
                }
                if (!parse_signal(argv[++i], initial_signal)) {
                    std::wcerr << L"expire: invalid signal: " << argv[i] << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }
            if (arg.rfind(L"-k=", 0) == 0 || arg.rfind(L"--kill-after=", 0) == 0) {
                size_t eq = arg.find(L'=');
                if (!parse_duration(arg.substr(eq + 1), kill_after_seconds)) {
                    std::wcerr << L"expire: invalid duration: " << arg.substr(eq + 1) << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }
            if (arg == L"-k" || arg == L"--kill-after") {
                if (i + 1 >= argc) {
                    std::wcerr << L"expire: option requires an argument -- '" << arg << L"'\n";
                    return EXIT_INTERNAL_ERROR;
                }
                if (!parse_duration(argv[++i], kill_after_seconds)) {
                    std::wcerr << L"expire: invalid duration: " << argv[i] << L"\n";
                    return EXIT_INTERNAL_ERROR;
                }
                continue;
            }

            std::wcerr << L"expire: invalid option -- '" << arg << L"'\n"
                       << L"Try 'expire --help' for more information.\n";
            return EXIT_INTERNAL_ERROR;
        }

        if (!parse_duration(arg, timeout_seconds)) {
            std::wcerr << L"expire: invalid duration: " << arg << L"\n";
            return EXIT_INTERNAL_ERROR;
        }

        cmd_idx = i + 1;
        break;
    }

    if (cmd_idx >= argc) {
        std::wcerr << L"expire: missing command\n"
                   << L"Try 'expire --help' for more information.\n";
        return EXIT_INTERNAL_ERROR;
    }

    std::wstring raw_cmd = argv[cmd_idx];
    std::wstring resolved_exe = resolve_executable(raw_cmd);

    // FIX 1: Detect batch files (.bat / .cmd) and wrap via cmd.exe /c
    bool batch_script = !resolved_exe.empty() && is_batch_file(resolved_exe);
    std::wstring cmdline;

    if (batch_script) {
        cmdline = L"cmd.exe /c " + escape_win32_arg(resolved_exe);
    } else if (!resolved_exe.empty()) {
        cmdline = escape_win32_arg(resolved_exe);
    } else {
        cmdline = escape_win32_arg(raw_cmd);
    }

    for (int k = cmd_idx + 1; k < argc; ++k) {
        cmdline += L" " + escape_win32_arg(argv[k]);
    }

    // Assign process tree to a Win32 Job Object for clean process tree termination
    ScopedHandle hJob(CreateJobObjectW(NULL, NULL));
    if (hJob.is_valid()) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!foreground) {
            jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        }
        SetInformationJobObject(hJob.get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmd_buf(cmdline.begin(), cmdline.end());
    cmd_buf.push_back(L'\0');

    LPCWSTR lpAppName = (!resolved_exe.empty() && !batch_script) ? resolved_exe.c_str() : NULL;
    DWORD creation_flags = CREATE_SUSPENDED;
    if (!foreground) {
        creation_flags |= CREATE_NEW_PROCESS_GROUP;
    }

    BOOL created = CreateProcessW(lpAppName, cmd_buf.data(), NULL, NULL, TRUE,
                                  creation_flags, NULL, NULL, &si, &pi);

    if (!created) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            std::wcerr << L"expire: failed to run command '" << raw_cmd << L"': No such file or directory\n";
            return EXIT_CMD_NOT_FOUND;
        }
        std::wcerr << L"expire: failed to run command '" << raw_cmd << L"': Access denied or invocation failed\n";
        return EXIT_CANNOT_INVOKE;
    }

    ScopedHandle hProcess(pi.hProcess);
    ScopedHandle hThread(pi.hThread);

    // FIX 2: Gracefully handle nested job assignment failures in CI/CD runners
    bool assigned_to_job = false;
    if (hJob.is_valid()) {
        if (AssignProcessToJobObject(hJob.get(), hProcess.get())) {
            assigned_to_job = true;
        }
    }

    // Ignore CTRL_C in expire process so signals route directly to child
    SetConsoleCtrlHandler(NULL, TRUE);

    ResumeThread(hThread.get());

    HANDLE active_job_handle = assigned_to_job ? hJob.get() : INVALID_HANDLE_VALUE;

    if (timeout_seconds <= 0.0) {
        // Infinite wait (timeout disabled)
        WaitForSingleObject(hProcess.get(), INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(hProcess.get(), &exit_code);
        return static_cast<int>(exit_code);
    }

    DWORD main_timeout_ms = static_cast<DWORD>(timeout_seconds * 1000.0);
    DWORD wait_res = WaitForSingleObject(hProcess.get(), main_timeout_ms);

    if (wait_res == WAIT_OBJECT_0) {
        // Command completed within timeout limit
        DWORD exit_code = 0;
        GetExitCodeProcess(hProcess.get(), &exit_code);
        return static_cast<int>(exit_code);
    }

    // FIX 3: Timeout expired - dispatch signals reliably
    dispatch_win32_signal(active_job_handle, hProcess.get(), pi.dwProcessId, initial_signal, verbose);

    if (kill_after_seconds >= 0.0) {
        DWORD kill_after_ms = static_cast<DWORD>(kill_after_seconds * 1000.0);
        DWORD wait_kill_res = WaitForSingleObject(hProcess.get(), kill_after_ms);
        if (wait_kill_res == WAIT_TIMEOUT) {
            dispatch_win32_signal(active_job_handle, hProcess.get(), pi.dwProcessId, SIG_KILL, verbose);
            WaitForSingleObject(hProcess.get(), 1000);
        }
    } else {
        WaitForSingleObject(hProcess.get(), 1000);
    }

    if (preserve_status) {
        DWORD exit_code = 0;
        GetExitCodeProcess(hProcess.get(), &exit_code);
        return static_cast<int>(exit_code);
    }

    return EXIT_TIMEOUT_EXPIRED;
}
