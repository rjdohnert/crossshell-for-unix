class ExecutionDispatcher {
public:
    static int Execute(const RunconConfig& cfg) {
        RunconConfig localCfg = cfg;
        localCfg.command = ResolveExecutableInCommand(cfg.command);

        fs::path destDir = localCfg.workingDirectory.empty() ? fs::current_path() : fs::path(localCfg.workingDirectory);
        fs::path targetDir = destDir;
        fs::path tempWorkspace;
        std::map<fs::path, FileSnapshot> initialSnapshot;
        std::vector<wchar_t> sandboxedEnv;

        if (localCfg.level != IsolationLevel::Normal) {
            auto epoch = std::chrono::steady_clock::now().time_since_epoch().count();
            tempWorkspace = fs::temp_directory_path() / (L"runcon_ws_" + std::to_wstring(epoch));
            fs::create_directories(tempWorkspace);
            
            {
                std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
                GlobalState::activeWorkspace = tempWorkspace;
            }
            
            targetDir = tempWorkspace;
            SecurityEngine::GrantAppContainerAccess(tempWorkspace, GENERIC_ALL);
            sandboxedEnv = SecurityEngine::CreateSandboxedEnvironmentBlock(tempWorkspace);

            if (localCfg.stageCurrentDir) {
                if (localCfg.verbose) std::wcout << L"\033[33m[*] Staging workspace directory into sandbox...\033[0m\n";
                for (const auto& entry : fs::directory_iterator(destDir)) {
                    if (entry.path() != tempWorkspace) {
                        std::error_code ec;
                        fs::path destination = tempWorkspace / entry.path().filename();
                        fs::copy(entry.path(), destination, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
                    }
                }
            }

            if (localCfg.level == IsolationLevel::PromptCommit) {
                std::error_code ec;
                for (const auto& entry : fs::recursive_directory_iterator(tempWorkspace, ec)) {
                    if (fs::is_regular_file(entry.status())) {
                        fs::path rel = fs::relative(entry.path(), tempWorkspace);
                        initialSnapshot[rel] = { fs::last_write_time(entry.path(), ec), fs::file_size(entry.path(), ec) };
                    }
                }
            }
        }

        ScopedHandle hJob(SecurityEngine::CreateJobSandbox(localCfg.level));
        {
            std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
            GlobalState::activeJob = hJob.get();
        }

        if (localCfg.level != IsolationLevel::Normal) {
            SecurityEngine::EnsureBinaryReadable(localCfg.command);
        }

        PSID appContainerSid = nullptr;
        std::wstring acProfileName;
        std::vector<SID_AND_ATTRIBUTES> capabilities;
        PSID pNetClientSid = nullptr;
        PSID pNetClientServerSid = nullptr;
        PSID pPrivateNetworkClientServerSid = nullptr;

        if (localCfg.level != IsolationLevel::Normal) {
            acProfileName = L"runcon_ac_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
            {
                std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
                GlobalState::appContainerProfileName = acProfileName;
            }

            HRESULT hr = CreateAppContainerProfile(acProfileName.c_str(), acProfileName.c_str(), acProfileName.c_str(), nullptr, 0, &appContainerSid);
            if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
                DeriveAppContainerSidFromAppContainerName(acProfileName.c_str(), &appContainerSid);
            }

            // Strict Fail-Closed Security Policy
            if (!appContainerSid) {
                std::wcerr << L"\033[31m[-] Fatal: Failed to initialize AppContainer security context (HRESULT: " 
                           << std::hex << hr << L"). Execution aborted.\033[0m\n";
                SafeRemoveAll(tempWorkspace);
                return 1;
            }

            if (localCfg.level != IsolationLevel::TotalLockdown) {
                if (ConvertStringSidToSidW(L"S-1-15-3-1", &pNetClientSid)) { // internetClient capability
                    SID_AND_ATTRIBUTES sa = { 0 };
                    sa.Sid = pNetClientSid;
                    sa.Attributes = SE_GROUP_ENABLED;
                    capabilities.push_back(sa);
                }
                if (ConvertStringSidToSidW(L"S-1-15-3-2", &pNetClientServerSid)) { // internetClientServer capability
                    SID_AND_ATTRIBUTES sa = { 0 };
                    sa.Sid = pNetClientServerSid;
                    sa.Attributes = SE_GROUP_ENABLED;
                    capabilities.push_back(sa);
                }
                if (ConvertStringSidToSidW(L"S-1-15-3-3", &pPrivateNetworkClientServerSid)) { // privateNetworkClientServer capability
                    SID_AND_ATTRIBUTES sa = { 0 };
                    sa.Sid = pPrivateNetworkClientServerSid;
                    sa.Attributes = SE_GROUP_ENABLED;
                    capabilities.push_back(sa);
                }
            }

            if (localCfg.verbose) {
                if (localCfg.level == IsolationLevel::TotalLockdown) {
                    std::wcout << L"\033[32m[+] Network Lockdown: Zero-Capability AppContainer active (WFP socket blocking).\033[0m\n";
                } else {
                    std::wcout << L"\033[32m[+] AppContainer active with internetClient capability.\033[0m\n";
                }
            }
        }

        // Validate Standard Handles
        std::vector<HANDLE> validInheritHandles;
        for (DWORD stdId : { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE }) {
            HANDLE h = GetStdHandle(stdId);
            if (h && h != INVALID_HANDLE_VALUE) {
                SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
                validInheritHandles.push_back(h);
            }
        }

        DWORD attrCount = 0;
        if (appContainerSid) attrCount++;
        if (!validInheritHandles.empty()) attrCount++;

        SIZE_T attrSize = 0;
        if (attrCount > 0) {
            InitializeProcThreadAttributeList(nullptr, attrCount, 0, &attrSize);
        }

        std::vector<BYTE> attrBuffer(attrSize);
        PPROC_THREAD_ATTRIBUTE_LIST pAttrList = attrSize ? reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrBuffer.data()) : nullptr;
        SECURITY_CAPABILITIES secCaps = { 0 };

        if (pAttrList) {
            InitializeProcThreadAttributeList(pAttrList, attrCount, 0, &attrSize);

            if (!validInheritHandles.empty()) {
                UpdateProcThreadAttribute(pAttrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, validInheritHandles.data(), 
                                           validInheritHandles.size() * sizeof(HANDLE), nullptr, nullptr);
            }

            if (appContainerSid) {
                secCaps.AppContainerSid = appContainerSid;
                secCaps.Capabilities = capabilities.empty() ? nullptr : capabilities.data();
                secCaps.CapabilityCount = static_cast<DWORD>(capabilities.size());
                UpdateProcThreadAttribute(pAttrList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &secCaps, sizeof(secCaps), nullptr, nullptr);
            }
        }

        STARTUPINFOEXW siEx = { 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        siEx.StartupInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        siEx.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        siEx.StartupInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
        siEx.lpAttributeList = pAttrList;

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdLine(localCfg.command.begin(), localCfg.command.end());
        cmdLine.push_back(L'\0');

        DWORD creationFlags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | (pAttrList ? EXTENDED_STARTUPINFO_PRESENT : 0);
        LPVOID pEnvironment = sandboxedEnv.empty() ? nullptr : sandboxedEnv.data();

        if (localCfg.verbose) {
            std::wcout << L"[*] Target Command: " << localCfg.command << L"\n";
            std::wcout << L"[*] Active Sandbox Directory: " << targetDir.wstring() << L"\n";
        }

        BOOL bCreated = CreateProcessW(
            nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
            creationFlags, pEnvironment, targetDir.c_str(), &siEx.StartupInfo, &pi
        );

        if (!bCreated) {
            DWORD err = GetLastError();
            std::wcerr << L"\033[31m[-] Execution failed with Win32 Error: " << err << L"\033[0m\n";
            if (pNetClientSid) LocalFree(pNetClientSid);
            if (pNetClientServerSid) LocalFree(pNetClientServerSid);
            if (pPrivateNetworkClientServerSid) LocalFree(pPrivateNetworkClientServerSid);
            if (appContainerSid) {
                FreeSid(appContainerSid);
                DeleteAppContainerProfile(acProfileName.c_str());
            }
            if (pAttrList) DeleteProcThreadAttributeList(pAttrList);
            SafeRemoveAll(tempWorkspace);
            
            std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
            GlobalState::activeJob = nullptr;
            GlobalState::appContainerProfileName.clear();
            return 1;
        }

        ScopedHandle hProc(pi.hProcess);
        ScopedHandle hThrd(pi.hThread);

        if (hJob.get() && !AssignProcessToJobObject(hJob.get(), pi.hProcess)) {
            std::wcerr << L"\033[31m[-] Failed to assign process to the sandbox job.\033[0m\n";
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, INFINITE);
            if (pAttrList) DeleteProcThreadAttributeList(pAttrList);
            if (pNetClientSid) LocalFree(pNetClientSid);
            if (pNetClientServerSid) LocalFree(pNetClientServerSid);
            if (pPrivateNetworkClientServerSid) LocalFree(pPrivateNetworkClientServerSid);
            if (appContainerSid) {
                FreeSid(appContainerSid);
                DeleteAppContainerProfile(acProfileName.c_str());
            }
            SafeRemoveAll(tempWorkspace);
            return 1;
        }

        // Resume process execution
        ResumeThread(pi.hThread);

        // Await process termination
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        // The direct process may have spawned descendants. Drain the job before
        // deleting the AppContainer profile or its temporary workspace.
        if (hJob.get()) {
            TerminateJobObject(hJob.get(), exitCode);
            DrainJobObject(hJob.get(), 2000);
        }

        // Cleanup AppContainer profile and capabilities
        if (pNetClientSid) LocalFree(pNetClientSid);
        if (pNetClientServerSid) LocalFree(pNetClientServerSid);
        if (pPrivateNetworkClientServerSid) LocalFree(pPrivateNetworkClientServerSid);
        if (appContainerSid) {
            FreeSid(appContainerSid);
            DeleteAppContainerProfile(acProfileName.c_str());
        }
        if (pAttrList) {
            DeleteProcThreadAttributeList(pAttrList);
        }
        
        {
            std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
            GlobalState::activeJob = nullptr;
            GlobalState::appContainerProfileName.clear();
        }

        std::wcout << L"\n\033[36m[*] Process terminated with exit code: \033[0m" << exitCode << L"\n";

        // Post-execution output policy handling
        ProcessOutputDispositions(localCfg, tempWorkspace, destDir, initialSnapshot);

        return static_cast<int>(exitCode);
    }

private:
    static void ProcessOutputDispositions(
        const RunconConfig& cfg,
        const fs::path& stageDir,
        const fs::path& destDir,
        const std::map<fs::path, FileSnapshot>& initialSnapshot) 
    {
        if (stageDir.empty() || !fs::exists(stageDir)) return;

        std::vector<fs::path> changedFiles;
        std::error_code ec;

        for (const auto& entry : fs::recursive_directory_iterator(stageDir, ec)) {
            if (fs::is_regular_file(entry.status())) {
                fs::path rel = fs::relative(entry.path(), stageDir);
                auto it = initialSnapshot.find(rel);
                if (it == initialSnapshot.end()) {
                    changedFiles.push_back(rel);
                } else {
                    if (fs::last_write_time(entry.path(), ec) != it->second.lastWrite ||
                        fs::file_size(entry.path(), ec) != it->second.size) {
                        changedFiles.push_back(rel);
                    }
                }
            }
        }

        switch (cfg.level) {
        case IsolationLevel::TotalLockdown:
        case IsolationLevel::EphemeralNet: {
            std::wcout << L"\033[33m[*] Policy Action: Discarded " << changedFiles.size() << L" transient modification(s).\033[0m\n";
            SafeRemoveAll(stageDir);
            break;
        }

        case IsolationLevel::TempWorkspace: {
            std::wcout << L"\033[32m[*] Level -4 Policy: Sandboxed output preserved in:\033[0m\n    " 
                       << stageDir.wstring() << L"\n";
            break;
        }

        case IsolationLevel::PromptCommit: {
            if (changedFiles.empty()) {
                std::wcout << L"[*] No file modifications or outputs detected.\n";
                SafeRemoveAll(stageDir);
                break;
            }

            std::wcout << L"\n\033[93m[?] Staged Changes Summary (" << changedFiles.size() << L" file(s) modified/created):\033[0m\n";
            for (const auto& rel : changedFiles) {
                fs::path fullPath = stageDir / rel;
                std::wcout << L"    [+] " << rel.wstring() 
                           << L" (" << fs::file_size(fullPath, ec) << L" bytes)\n";
            }

            std::wcout << L"\n\033[93m[?] Commit staged modifications back to " << destDir.wstring() << L"? [y/N]: \033[0m";
            std::wstring choice;
            std::wcin >> choice;

            if (choice == L"y" || choice == L"Y") {
                for (const auto& rel : changedFiles) {
                    fs::path source = stageDir / rel;
                    fs::path target = destDir / rel;
                    fs::create_directories(target.parent_path(), ec);
                    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
                }
                std::wcout << L"\033[32m[+] Staged changes merged successfully.\033[0m\n";
            } else {
                std::wcout << L"\033[31m[-] Discarding all sandboxed modifications.\033[0m\n";
            }
            SafeRemoveAll(stageDir);
            break;
        }

        default:
            break;
        }
    }
};

// --- CLI Parsing & Main Entry ---
int wmain(int argc, wchar_t* argv[]) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    RunconConfig cfg;
    bool parsingCommand = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (parsingCommand) {
            if (!cfg.command.empty()) cfg.command += L" ";
            cfg.command += EscapeArgument(arg);
            continue;
        }

        if (arg == L"-h" || arg == L"--help") {
            PrintHelp();
            return 0;
        } else if (arg == L"-V" || arg == L"--version") {
            PrintVersion();
            return 0;
        } else if (arg == L"-5") {
            cfg.level = IsolationLevel::Normal;
        } else if (arg == L"-4") {
            cfg.level = IsolationLevel::TempWorkspace;
        } else if (arg == L"-3") {
            cfg.level = IsolationLevel::PromptCommit;
        } else if (arg == L"-2") {
            cfg.level = IsolationLevel::EphemeralNet;
        } else if (arg == L"-1") {
            cfg.level = IsolationLevel::TotalLockdown;
        } else if (arg == L"-s" || arg == L"--stage") {
            cfg.stageCurrentDir = true;
        } else if (arg == L"-v" || arg == L"--verbose") {
            cfg.verbose = true;
        } else if ((arg == L"-d" || arg == L"--dir") && (i + 1 < argc)) {
            cfg.workingDirectory = argv[++i];
        } else if (arg == L"--") {
            parsingCommand = true;
        } else {
            parsingCommand = true;
            cfg.command += EscapeArgument(arg);
        }
    }

    if (cfg.command.empty()) {
        std::wcerr << L"\033[31m[-] Error: No execution target specified.\033[0m\n";
        std::wcerr << L"    Run 'conexec --help' for syntax.\n";
        return 1;
    }

    return ExecutionDispatcher::Execute(cfg);
}