#include "xmic_common.hpp"

int wmain(int argc, wchar_t* argv[]) {
    ConsoleEncoding consoleEncoding = ConsoleEncoding::Utf16;
    if (!ConfigureConsoleOutputMode(consoleEncoding)) {
        std::wcerr << L"[!] Failed to configure default console output mode." << std::endl;
        return EXIT_BAD_ARGS;
    }

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::wcerr << L"[!] COM Initialization failed." << std::endl;
        return EXIT_WMI_FAILURE;
    }
    bool coInitialized = SUCCEEDED(hr);

    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    // If COM security was already initialized by another component (host/injected DLL),
    // RPC_E_TOO_LATE is expected and should not be treated as fatal.
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        std::wcerr << L"[!] CoInitializeSecurity failed: " << DescribeWmiHResult(hr) << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_WMI_FAILURE;
    }

    if (argc >= 2 && std::wstring(argv[1]) == L"shell") {
        std::wstring executablePath = argv[0] ? argv[0] : L"xmic.exe";
        std::wstring resolvedExecutablePath;
        if (!ResolveExecutablePath(executablePath, resolvedExecutablePath)) {
            std::wcerr << L"[!] Failed to resolve xmic executable path for shell mode." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_WMI_FAILURE;
        }

        int shellExitCode = RunInteractiveShell(resolvedExecutablePath);
        if (coInitialized) CoUninitialize();
        return shellExitCode;
    }

    if (argc < 2) {
        PrintHelp();
        if (coInitialized) CoUninitialize();
        return EXIT_OK;
    }

    if (std::wstring(argv[1]) == L"selftest" || std::wstring(argv[1]) == L"test-wsl") {
        int selfTestExit = RunWslDetectionSelfTests();
        if (coInitialized) CoUninitialize();
        return selfTestExit;
    }

    int benchmarkRuns = 1;
    bool benchmarkQuiet = false;
    std::vector<std::wstring> benchmarkForwardArgs;
    benchmarkForwardArgs.reserve((argc > 1) ? static_cast<size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == kBenchmarkRunsOption) {
            int parsedRuns = 0;
            std::wstring parseError;
            if (!TryParseBenchmarkRunsValue(i + 1, argc, argv, parsedRuns, parseError)) {
                std::wcerr << L"[!] " << parseError << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            benchmarkRuns = parsedRuns;
            ++i;
            continue;
        }

        if (arg == kBenchmarkQuietOption) {
            benchmarkQuiet = true;
            continue;
        }

        benchmarkForwardArgs.push_back(arg);
    }

    int argIndex = 1;
    std::wstring wmiLocale;
    WmiSecurityConfig securityConfig;
    WmiTargetConfig targetConfig;
    struct PasswordScrubber {
        std::wstring* value;
        ~PasswordScrubber() {
            if (value != nullptr && !value->empty()) {
                SecureZeroMemory(&(*value)[0], value->size() * sizeof(wchar_t));
                value->clear();
            }
        }
    } passwordScrubber{ &targetConfig.password };
    StdoutRedirector redirector;
    bool noHeader = false;
    OutputFormat outputFormat = OutputFormat::Table;
    bool forcePasswordPrompt = false;
    bool forcePasswordPromptConfirm = false;
    bool timingEnabled = false;
    std::wstring scriptFilePath;
    bool scriptStopOnError = false;
    bool scriptEcho = true;

    while (argIndex < argc) {
        std::wstring option = argv[argIndex];

        if (option == L"--locale") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --locale." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            wmiLocale = argv[argIndex + 1];
            argIndex += 2;
            continue;
        }

        if (option == L"--console-encoding") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --console-encoding." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            if (!TryParseConsoleEncoding(argv[argIndex + 1], consoleEncoding)) {
                std::wcerr << L"[!] Invalid --console-encoding value. Use utf16 or utf8." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            if (!ConfigureConsoleOutputMode(consoleEncoding)) {
                std::wcerr << L"[!] Failed to configure requested console output mode." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            argIndex += 2;
            continue;
        }

        if (option == L"--wmi-security") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --wmi-security." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            if (!TryParseWmiSecurity(argv[argIndex + 1], securityConfig)) {
                std::wcerr << L"[!] Invalid --wmi-security value." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            argIndex += 2;
            continue;
        }

        if (option == L"--transport") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --transport." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            if (!TryParseTransportMode(argv[argIndex + 1], targetConfig.transport)) {
                std::wcerr << L"[!] Invalid --transport value. Use dcom, winrm, or ssh." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            argIndex += 2;
            continue;
        }

        if (option == L"--ssh-port") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --ssh-port." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            try {
                targetConfig.sshPort = std::stoi(argv[argIndex + 1]);
            } catch (...) {
                std::wcerr << L"[!] Invalid --ssh-port value." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            if (targetConfig.sshPort < 1 || targetConfig.sshPort > 65535) {
                std::wcerr << L"[!] --ssh-port must be between 1 and 65535." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            argIndex += 2;
            continue;
        }

        if (option == L"--ssh-key") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --ssh-key." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            targetConfig.sshKeyFile = argv[argIndex + 1];
            argIndex += 2;
            continue;
        }

        if (option == L"--ssh-strict-host-key") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --ssh-strict-host-key." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            if (!TryParseSshStrictHostKeyCheckingMode(argv[argIndex + 1], targetConfig.sshStrictHostKeyChecking)) {
                std::wcerr << L"[!] Invalid --ssh-strict-host-key value. Use yes, no, ask, or accept-new." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            argIndex += 2;
            continue;
        }

        if (option == L"--ssh-known-hosts") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --ssh-known-hosts." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            targetConfig.sshKnownHostsFile = argv[argIndex + 1];
            argIndex += 2;
            continue;
        }

        if (option == L"--ssh-proxy-jump") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --ssh-proxy-jump." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            targetConfig.sshProxyJump = argv[argIndex + 1];
            argIndex += 2;
            continue;
        }

        if (option == L"--ssh-prompt-key-passphrase") {
            targetConfig.sshPromptKeyPassphrase = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--winrm-backend") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --winrm-backend." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            if (!TryParseWinRmBackendMode(argv[argIndex + 1], targetConfig.winrmBackend)) {
                std::wcerr << L"[!] Invalid --winrm-backend value. Use powershell or native." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            argIndex += 2;
            continue;
        }

        if (option == L"--format") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --format." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            if (!TryParseOutputFormat(argv[argIndex + 1], outputFormat)) {
                std::wcerr << L"[!] Invalid --format value." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            argIndex += 2;
            continue;
        }

        if (option == L"--verbose" || option == L"-v") {
            g_outputOptions.verbose = true;
            argIndex += 1;
            continue;
        }

        if (option == kBenchmarkRunsOption) {
            int parsedRuns = 0;
            std::wstring parseError;
            if (!TryParseBenchmarkRunsValue(argIndex + 1, argc, argv, parsedRuns, parseError)) {
                std::wcerr << L"[!] " << parseError << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            benchmarkRuns = parsedRuns;
            argIndex += 2;
            continue;
        }

        if (option == kBenchmarkQuietOption) {
            benchmarkQuiet = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--timing") {
            timingEnabled = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--no-header") {
            noHeader = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--script") {
            if (argIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --script." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            scriptFilePath = argv[argIndex + 1];
            argIndex += 2;
            continue;
        }

        if (option == L"--script-stop-on-error") {
            scriptStopOnError = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--script-no-echo") {
            scriptEcho = false;
            argIndex += 1;
            continue;
        }

        if (option == L"--namespace" || _wcsnicmp(option.c_str(), L"/NAMESPACE:", 11) == 0) {
            if (option == L"--namespace") {
                if (argIndex + 1 >= argc) {
                    std::wcerr << L"[!] Missing value for --namespace." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                targetConfig.wmiNamespace = argv[argIndex + 1];
                argIndex += 2;
            } else {
                targetConfig.wmiNamespace = option.substr(11);
                argIndex += 1;
            }
            continue;
        }

        if (option == L"--output" || _wcsnicmp(option.c_str(), L"/OUTPUT:", 8) == 0) {
            std::wstring file;
            if (option == L"--output") {
                if (argIndex + 1 >= argc) {
                    std::wcerr << L"[!] Missing value for --output." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                file = argv[argIndex + 1];
                argIndex += 2;
            } else {
                file = option.substr(8);
                argIndex += 1;
            }
            if (!redirector.RedirectToFile(file)) {
                std::wcerr << L"[!] Failed to open output file: " << file << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            continue;
        }

        if (option == L"--node" || _wcsnicmp(option.c_str(), L"/NODE:", 6) == 0) {
            if (option == L"--node") {
                if (argIndex + 1 >= argc) {
                    std::wcerr << L"[!] Missing value for --node." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                targetConfig.node = argv[argIndex + 1];
                argIndex += 2;
            } else {
                targetConfig.node = option.substr(6);
                argIndex += 1;
            }
            continue;
        }

        if (option == L"--user" || _wcsnicmp(option.c_str(), L"/USER:", 6) == 0) {
            if (option == L"--user") {
                if (argIndex + 1 >= argc) {
                    std::wcerr << L"[!] Missing value for --user." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                targetConfig.user = argv[argIndex + 1];
                argIndex += 2;
            } else {
                targetConfig.user = option.substr(6);
                argIndex += 1;
            }
            continue;
        }

        if (option == L"--password" || _wcsnicmp(option.c_str(), L"/PASSWORD:", 10) == 0) {
            if (option == L"--password") {
                if (argIndex + 1 >= argc) {
                    std::wcerr << L"[!] Missing value for --password." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                targetConfig.password = argv[argIndex + 1];
                argIndex += 2;
            } else {
                targetConfig.password = option.substr(10);
                argIndex += 1;
            }
            continue;
        }

        if (option == L"--prompt-password") {
            forcePasswordPrompt = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--prompt-password-confirm") {
            forcePasswordPromptConfirm = true;
            argIndex += 1;
            continue;
        }

        if (option == L"--domain" || _wcsnicmp(option.c_str(), L"/DOMAIN:", 8) == 0) {
            if (option == L"--domain") {
                if (argIndex + 1 >= argc) {
                    std::wcerr << L"[!] Missing value for --domain." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                targetConfig.domain = argv[argIndex + 1];
                argIndex += 2;
            } else {
                targetConfig.domain = option.substr(8);
                argIndex += 1;
            }
            continue;
        }

        break;
    }

    g_outputOptions.format = outputFormat;
    g_outputOptions.noHeader = noHeader;

    if (scriptFilePath.empty() && scriptStopOnError) {
        std::wcerr << L"[!] --script-stop-on-error requires --script <FILE>." << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }

    if (scriptFilePath.empty() && !scriptEcho) {
        std::wcerr << L"[!] --script-no-echo requires --script <FILE>." << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }

    if (forcePasswordPromptConfirm && !targetConfig.password.empty()) {
        std::wcerr << L"[!] --prompt-password-confirm cannot be used with --password." << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }

    if (targetConfig.transport == TransportMode::WinRm &&
        targetConfig.winrmBackend == WinRmBackendMode::PowerShell &&
        (!targetConfig.user.empty() || !targetConfig.password.empty() || !targetConfig.domain.empty() ||
         forcePasswordPrompt || forcePasswordPromptConfirm)) {
        std::wcerr << L"[!] WinRM PowerShell backend does not accept explicit credentials. Use current logon token, or switch to --winrm-backend native." << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }

    if (targetConfig.transport == TransportMode::Ssh) {
        if (targetConfig.node.empty() || targetConfig.node == L".") {
            std::wcerr << L"[!] SSH transport requires --node <HOST>." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        if (!forcePasswordPrompt && !forcePasswordPromptConfirm && !targetConfig.password.empty()) {
            std::wcerr << L"[!] SSH transport does not use --password. Use SSH key/agent authentication." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        if (forcePasswordPrompt || forcePasswordPromptConfirm) {
            std::wcerr << L"[!] SSH transport does not support --prompt-password options. Use SSH key/agent authentication." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        if (!targetConfig.domain.empty()) {
            std::wcerr << L"[!] SSH transport does not use --domain." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        if (targetConfig.sshPromptKeyPassphrase && targetConfig.sshKeyFile.empty()) {
            std::wcerr << L"[!] --ssh-prompt-key-passphrase requires --ssh-key <FILE>." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
    } else {
        if (!targetConfig.sshKeyFile.empty() ||
            !targetConfig.sshStrictHostKeyChecking.empty() ||
            !targetConfig.sshKnownHostsFile.empty() ||
            !targetConfig.sshProxyJump.empty() ||
            targetConfig.sshPromptKeyPassphrase ||
            targetConfig.sshPort != 22) {
            std::wcerr << L"[!] SSH-specific options require --transport ssh." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
    }

    bool isRemoteTarget = !(targetConfig.node.empty() || targetConfig.node == L".");
    bool shouldPromptForPassword =
        (targetConfig.transport != TransportMode::Ssh) &&
        (forcePasswordPromptConfirm ||
         forcePasswordPrompt ||
         (isRemoteTarget && !targetConfig.user.empty() && targetConfig.password.empty()));

    std::wstring benchmarkValidationError;
    bool interactivePasswordRequired = forcePasswordPrompt || forcePasswordPromptConfirm || shouldPromptForPassword;
    if (!ValidateBenchmarkOptions(benchmarkRuns, benchmarkQuiet, interactivePasswordRequired, benchmarkValidationError)) {
        std::wcerr << L"[!] " << benchmarkValidationError << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }

    if (shouldPromptForPassword) {
        if (targetConfig.user.empty()) {
            std::wcerr << L"[!] Password prompt mode requires --user for remote authentication." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        std::wstring prompt = L"Enter remote password";
        if (!targetConfig.user.empty()) {
            prompt += L" for " + targetConfig.user;
        }
        if (isRemoteTarget) {
            prompt += L"@" + targetConfig.node;
        }
        prompt += L": ";

        std::wstring firstEntry;
        std::wstring promptError;
        if (!ReadHiddenConsoleLine(prompt, firstEntry, promptError)) {
            std::wcerr << L"[!] " << promptError << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        if (firstEntry.empty()) {
            std::wcerr << L"[!] Password entry cannot be empty." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        if (forcePasswordPromptConfirm) {
            std::wstring secondEntry;
            if (!ReadHiddenConsoleLine(L"Confirm remote password: ", secondEntry, promptError)) {
                SecureZeroMemory(&firstEntry[0], firstEntry.size() * sizeof(wchar_t));
                std::wcerr << L"[!] " << promptError << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            if (firstEntry != secondEntry) {
                if (!firstEntry.empty()) {
                    SecureZeroMemory(&firstEntry[0], firstEntry.size() * sizeof(wchar_t));
                }
                if (!secondEntry.empty()) {
                    SecureZeroMemory(&secondEntry[0], secondEntry.size() * sizeof(wchar_t));
                }
                std::wcerr << L"[!] Password confirmation does not match." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            if (!secondEntry.empty()) {
                SecureZeroMemory(&secondEntry[0], secondEntry.size() * sizeof(wchar_t));
            }
        }

        targetConfig.password = firstEntry;
        if (!firstEntry.empty()) {
            SecureZeroMemory(&firstEntry[0], firstEntry.size() * sizeof(wchar_t));
        }
    }

    if (targetConfig.transport == TransportMode::Ssh && targetConfig.sshPromptKeyPassphrase) {
        std::wstring prompt = L"Enter SSH key passphrase";
        if (!targetConfig.sshKeyFile.empty()) {
            prompt += L" for " + targetConfig.sshKeyFile;
        }
        if (isRemoteTarget) {
            prompt += L"@" + targetConfig.node;
        }
        prompt += L": ";

        std::wstring passphrase;
        std::wstring promptError;
        if (!ReadHiddenConsoleLine(prompt, passphrase, promptError)) {
            std::wcerr << L"[!] " << promptError << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        targetConfig.sshKeyPassphrase = passphrase;
        if (!passphrase.empty()) {
            SecureZeroMemory(&passphrase[0], passphrase.size() * sizeof(wchar_t));
        }
    }

    if (argIndex >= argc) {
        if (!scriptFilePath.empty()) {
            int inheritedScriptDepth = 0;
            std::wstring inheritedScriptDepthRaw;
            if (TryGetEnvironmentVariableValue(kScriptDepthEnvVar, inheritedScriptDepthRaw)) {
                if (!TryParseScriptDepth(inheritedScriptDepthRaw, inheritedScriptDepth)) {
                    std::wcerr << L"[!] Invalid inherited script depth context." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
            }

            std::wstring inheritedScriptStack;
            TryGetEnvironmentVariableValue(kScriptStackEnvVar, inheritedScriptStack);

            std::wstring normalizedScriptPath = NormalizePathForComparison(scriptFilePath);
            if (ContainsPathInSemicolonList(inheritedScriptStack, normalizedScriptPath)) {
                std::wcerr << L"[!] Script recursion/cycle detected for script: " << scriptFilePath << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }

            int scriptExitCode = EXIT_OK;
            bool scriptOk = ExecuteScriptFile(
                scriptFilePath,
                argv[0],
                scriptStopOnError,
                scriptEcho,
                inheritedScriptDepth,
                inheritedScriptStack,
                scriptExitCode);
            if (coInitialized) CoUninitialize();
            return scriptOk ? EXIT_OK : scriptExitCode;
        }

        PrintHelp();
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }

    std::wstring cmd = argv[argIndex];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), towlower);

    if (benchmarkRuns > 1) {
        if (benchmarkForwardArgs.empty()) {
            std::wcerr << L"[!] " << kBenchmarkMissingCommandMessage << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        std::wstring executablePath = argv[0] ? argv[0] : L"xmic.exe";
        std::wstring resolvedExecutablePath;
        if (!ResolveExecutablePath(executablePath, resolvedExecutablePath)) {
            std::wcerr << L"[!] Failed to resolve xmic executable path for benchmark mode." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_WMI_FAILURE;
        }

        double minMs = 1.0e300;
        double maxMs = 0.0;
        double totalMs = 0.0;
        std::vector<double> runSamples;
        runSamples.reserve(static_cast<size_t>(benchmarkRuns));

        std::wcerr << kBenchmarkBannerPrefix << cmd << kBenchmarkBannerSuffix << benchmarkRuns << kBenchmarkIterationSuffix << std::endl;
        for (int run = 1; run <= benchmarkRuns; ++run) {
            std::wstring launchError;
            int childExitCode = EXIT_OK;

            const auto runStart = std::chrono::steady_clock::now();
            bool launched = RunChildProcessAndWait(resolvedExecutablePath, benchmarkForwardArgs, launchError, childExitCode, benchmarkQuiet);
            const auto runEnd = std::chrono::steady_clock::now();

            if (!launched) {
                std::wcerr << L"[!] " << launchError << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_WMI_FAILURE;
            }

            double runMs = ElapsedMilliseconds(runStart, runEnd);
            minMs = (std::min)(minMs, runMs);
            maxMs = (std::max)(maxMs, runMs);
            totalMs += runMs;
            runSamples.push_back(runMs);

            if (childExitCode != EXIT_OK) {
                std::wcerr << L"[benchmark] Iteration " << run << L" exited with code " << childExitCode << L"." << std::endl;
                if (coInitialized) CoUninitialize();
                return childExitCode;
            }
        }

        const double avgMs = totalMs / static_cast<double>(benchmarkRuns);
        std::sort(runSamples.begin(), runSamples.end());

        double medianMs = 0.0;
        const size_t sampleCount = runSamples.size();
        if ((sampleCount % 2) == 1) {
            medianMs = runSamples[sampleCount / 2];
        } else {
            medianMs = (runSamples[(sampleCount / 2) - 1] + runSamples[sampleCount / 2]) / 2.0;
        }

        size_t p95Index = ((sampleCount * 95) + 99) / 100;
        if (p95Index == 0) {
            p95Index = 1;
        }
        if (p95Index > sampleCount) {
            p95Index = sampleCount;
        }
        const double p95Ms = runSamples[p95Index - 1];

        std::wcerr << kBenchmarkSummaryPrefix << benchmarkRuns
               << kBenchmarkSummaryMinLabel << std::fixed << std::setprecision(3) << minMs << kBenchmarkSummaryUnits
               << kBenchmarkSummaryAvgLabel << std::fixed << std::setprecision(3) << avgMs << kBenchmarkSummaryUnits
               << kBenchmarkSummaryMedLabel << std::fixed << std::setprecision(3) << medianMs << kBenchmarkSummaryUnits
               << kBenchmarkSummaryP95Label << std::fixed << std::setprecision(3) << p95Ms << kBenchmarkSummaryUnits
               << kBenchmarkSummaryMaxLabel << std::fixed << std::setprecision(3) << maxMs << kBenchmarkSummaryUnits
                   << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_OK;
    }

    std::wstring presetWhereClause;
    int presetArgIndex = argIndex + 1;
    if (presetArgIndex < argc && _wcsicmp(argv[presetArgIndex], L"where") == 0) {
        if (presetArgIndex + 1 >= argc) {
            std::wcerr << L"[!] Missing where clause expression after 'where'." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        presetWhereClause = argv[presetArgIndex + 1];
        presetArgIndex += 2;
    }

    // Accept output formatting flags after preset commands for WMIC-style invocation parity.
    int presetArgTailIndex = presetArgIndex;
    while (presetArgTailIndex < argc) {
        std::wstring trailingOption = argv[presetArgTailIndex];
        if (trailingOption == L"--format") {
            if (presetArgTailIndex + 1 >= argc) {
                std::wcerr << L"[!] Missing value for --format." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            if (!TryParseOutputFormat(argv[presetArgTailIndex + 1], outputFormat)) {
                std::wcerr << L"[!] Invalid --format value." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            presetArgTailIndex += 2;
            continue;
        }

        if (_wcsnicmp(trailingOption.c_str(), L"/FORMAT:", 8) == 0) {
            if (!TryParseOutputFormat(trailingOption.substr(8), outputFormat)) {
                std::wcerr << L"[!] Invalid /FORMAT value." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
            presetArgTailIndex += 1;
            continue;
        }

        if (trailingOption == L"--no-header") {
            noHeader = true;
            presetArgTailIndex += 1;
            continue;
        }

        if (trailingOption == L"--timing") {
            timingEnabled = true;
            presetArgTailIndex += 1;
            continue;
        }

        break;
    }

    g_outputOptions.format = outputFormat;
    g_outputOptions.noHeader = noHeader;

    if (cmd == L"help" || cmd == L"--help" || cmd == L"-h") {
        PrintHelp();
        if (coInitialized) CoUninitialize();
        return EXIT_OK;
    }

    const auto commandStart = std::chrono::steady_clock::now();

    bool success = true;

    if (cmd == L"os") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for os preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Caption, Version, OSArchitecture, NumberOfUsers, InstallDate FROM Win32_OperatingSystem", presetWhereClause),
            { L"Caption", L"Version", L"OSArchitecture", L"NumberOfUsers", L"InstallDate" },
            false,
            wmiLocale,
            { L"Caption", L"Version", L"OS Architecture", L"Number of Users", L"Install Date" },
            securityConfig
        );
    } 
    else if (cmd == L"cpu") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for cpu preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed, LoadPercentage FROM Win32_Processor", presetWhereClause),
            { L"Name", L"NumberOfCores", L"NumberOfLogicalProcessors", L"MaxClockSpeed", L"LoadPercentage" },
            true,
            wmiLocale,
            { L"Name", L"Physical Cores", L"Logical Processors", L"Max Clock", L"Load" },
            securityConfig
        );
    } 
    else if (cmd == L"ram") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for ram preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteRamStatusSummary(targetConfig, wmiLocale, securityConfig);
        if (!success) {
            g_lastNoData = true;
            if (g_outputOptions.format == OutputFormat::Table) {
                std::wcout << L"  No data available.\n" << std::endl;
            }
        }
        if (g_outputOptions.format == OutputFormat::Table) {
            std::wcout << L"\nHardware Slots:" << std::endl;
        }
        bool slotsSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT DeviceLocator, Capacity, Speed, Manufacturer FROM Win32_PhysicalMemory", presetWhereClause),
            { L"DeviceLocator", L"Capacity", L"Speed", L"Manufacturer" },
            true, wmiLocale, {}, securityConfig
        );
        success = success && slotsSuccess;
    } 
    else if (cmd == L"disk") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for disk preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteGetDiskPreset(targetConfig, presetWhereClause, wmiLocale, securityConfig);
    } 
    else if (cmd == L"extdisk" || cmd == L"external") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for extdisk preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT DeviceID, VolumeName, FileSystem, Size, FreeSpace, DriveType FROM Win32_LogicalDisk WHERE DriveType=2", presetWhereClause),
            { L"DeviceID", L"VolumeName", L"FileSystem", L"Size", L"FreeSpace", L"DriveType" },
            true, wmiLocale, {}, securityConfig
        );
    }
    else if (cmd == L"net") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for net preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Description, MACAddress, IPAddress FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=TRUE", presetWhereClause),
            { L"Description", L"MACAddress", L"IPAddress" },
            true,
            wmiLocale,
            { L"Adapter", L"MAC Address", L"IP Addresses" },
            securityConfig
        );
    } 
    else if (cmd == L"nicconfig") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for nicconfig preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Description, IPAddress, MACAddress, DHCPEnabled FROM Win32_NetworkAdapterConfiguration", presetWhereClause),
            { L"Description", L"IPAddress", L"MACAddress", L"DHCPEnabled" },
            true,
            wmiLocale,
            { L"Description", L"IP Addresses", L"MAC Address", L"DHCP Enabled" },
            securityConfig
        );
    }
    else if (cmd == L"gpu") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for gpu preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, DriverVersion, AdapterRAM FROM Win32_VideoController", presetWhereClause),
            { L"Name", L"DriverVersion", L"AdapterRAM" },
            true, wmiLocale, {}, securityConfig
        );
    }
    else if (cmd == L"bios" || cmd == L"firmware") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for bios preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Manufacturer, SMBIOSBIOSVersion, ReleaseDate, SerialNumber FROM Win32_BIOS", presetWhereClause),
            { L"Manufacturer", L"SMBIOSBIOSVersion", L"ReleaseDate", L"SerialNumber" },
            true, wmiLocale, {}, securityConfig
        );
    }
    else if (cmd == L"gswitch" || cmd == L"globalswitch") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport != TransportMode::DcomRpc) {
            bool osSwitchSuccess = ExecuteWMIQuery(
                targetConfig,
                L"SELECT DataExecutionPrevention_Available, DataExecutionPrevention_32BitApplications, DataExecutionPrevention_Drivers, PAEEnabled, Debug, Distributed, PortableOperatingSystem FROM Win32_OperatingSystem",
                {
                    L"DataExecutionPrevention_Available",
                    L"DataExecutionPrevention_32BitApplications",
                    L"DataExecutionPrevention_Drivers",
                    L"PAEEnabled",
                    L"Debug",
                    L"Distributed",
                    L"PortableOperatingSystem"
                },
                false,
                wmiLocale,
                {
                    L"DEP Available",
                    L"DEP for 32-bit Apps",
                    L"DEP for Drivers",
                    L"PAE Enabled",
                    L"Kernel Debug",
                    L"Distributed Build",
                    L"Portable OS"
                },
                securityConfig
            );

            bool systemSwitchSuccess = ExecuteWMIQuery(
                targetConfig,
                L"SELECT AutomaticManagedPagefile, AutomaticResetBootOption, AutomaticResetCapability, HypervisorPresent FROM Win32_ComputerSystem",
                {
                    L"AutomaticManagedPagefile",
                    L"AutomaticResetBootOption",
                    L"AutomaticResetCapability",
                    L"HypervisorPresent"
                },
                false,
                wmiLocale,
                {
                    L"Auto Managed Pagefile",
                    L"Auto Reset Boot Option",
                    L"Auto Reset Capability",
                    L"Hypervisor Present"
                },
                securityConfig
            );

            success = osSwitchSuccess && systemSwitchSuccess;
        } else {
            bool osSwitchSuccess = ExecuteWMISwitchStateQuery(
                targetConfig,
                L"Operating System Global Switches",
                L"SELECT DataExecutionPrevention_Available, DataExecutionPrevention_32BitApplications, DataExecutionPrevention_Drivers, PAEEnabled, Debug, Distributed, PortableOperatingSystem FROM Win32_OperatingSystem",
                {
                    { L"DataExecutionPrevention_Available", L"DEP Available" },
                    { L"DataExecutionPrevention_32BitApplications", L"DEP for 32-bit Apps" },
                    { L"DataExecutionPrevention_Drivers", L"DEP for Drivers" },
                    { L"PAEEnabled", L"PAE Enabled" },
                    { L"Debug", L"Kernel Debug" },
                    { L"Distributed", L"Distributed Build" },
                    { L"PortableOperatingSystem", L"Portable OS" }
                },
                wmiLocale, securityConfig
            );

            bool systemSwitchSuccess = ExecuteWMISwitchStateQuery(
                targetConfig,
                L"System Global Switches",
                L"SELECT AutomaticManagedPagefile, AutomaticResetBootOption, AutomaticResetCapability, HypervisorPresent FROM Win32_ComputerSystem",
                {
                    { L"AutomaticManagedPagefile", L"Auto Managed Pagefile" },
                    { L"AutomaticResetBootOption", L"Auto Reset Boot Option" },
                    { L"AutomaticResetCapability", L"Auto Reset Capability" },
                    { L"HypervisorPresent", L"Hypervisor Present" }
                },
                wmiLocale, securityConfig
            );

            success = osSwitchSuccess && systemSwitchSuccess;
        }
    }
    else if (cmd == L"vswitch" || cmd == L"virtualswitch") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport != TransportMode::DcomRpc) {
            bool featureSuccess = ExecuteWMIQuery(
                targetConfig,
                L"SELECT Name, InstallState FROM Win32_OptionalFeature WHERE Name='Microsoft-Hyper-V-All' OR Name='Microsoft-Hyper-V' OR Name='Microsoft-Hyper-V-Hypervisor' OR Name='Microsoft-Hyper-V-Services' OR Name='Microsoft-Hyper-V-Management-PowerShell' OR Name='VirtualMachinePlatform' OR Name='Microsoft-Windows-Subsystem-Linux' OR Name='HypervisorPlatform'",
                { L"Name", L"InstallState" },
                false,
                wmiLocale,
                { L"Feature", L"Install State" },
                securityConfig
            );

            bool platformSuccess = ExecuteWMIQuery(
                targetConfig,
                L"SELECT HypervisorPresent FROM Win32_ComputerSystem",
                { L"HypervisorPresent" },
                false,
                wmiLocale,
                { L"Hypervisor Present" },
                securityConfig
            );

            bool serviceSuccess = ExecuteWMIQuery(
                targetConfig,
                L"SELECT Name, State, StartMode FROM Win32_Service WHERE Name='vmms' OR Name='vmcompute' OR Name='LxssManager'",
                { L"Name", L"State", L"StartMode" },
                false,
                wmiLocale,
                { L"Service", L"State", L"Start Mode" },
                securityConfig
            );

            success = featureSuccess && platformSuccess && serviceSuccess;
        } else {
            success = ExecuteVirtualSwitchQuery(targetConfig, wmiLocale, securityConfig);
        }
    }
    else if (cmd == L"security" || cmd == L"wmisecurity") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport != TransportMode::DcomRpc) {
            std::wstring transportLabel = L"transport";
            if (targetConfig.transport == TransportMode::WinRm) transportLabel = L"winrm";
            else if (targetConfig.transport == TransportMode::Ssh) transportLabel = L"ssh";

            Table table;
            table.headers = { L"Security Level", L"Status", L"Details" };
            table.rows.push_back({ L"default", L"N/A", L"WMI COM security levels are DCOM-only; not applicable for " + transportLabel + L" transport." });
            table.rows.push_back({ L"connect", L"N/A", L"WMI COM security levels are DCOM-only; not applicable for " + transportLabel + L" transport." });
            table.rows.push_back({ L"call", L"N/A", L"WMI COM security levels are DCOM-only; not applicable for " + transportLabel + L" transport." });
            table.rows.push_back({ L"pkt", L"N/A", L"WMI COM security levels are DCOM-only; not applicable for " + transportLabel + L" transport." });
            table.rows.push_back({ L"pktprivacy", L"N/A", L"WMI COM security levels are DCOM-only; not applicable for " + transportLabel + L" transport." });

            if (g_outputOptions.format == OutputFormat::Table) {
                std::wcout << L"\nWMI Security Capability Check:" << std::endl;
            }
            table.Print();
        } else {
            PrintWmiSecurityAvailability(targetConfig, wmiLocale);
        }
        success = true;
    }
    else if (cmd == L"context" || cmd == L"wmiccontext" || cmd == L"wmic") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for context preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (g_outputOptions.format == OutputFormat::Table) {
            std::wcout << L"\nWMIC Context Summary:" << std::endl;
        }
        bool osContextSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT CSName, Caption, Version, OSArchitecture, LocalDateTime, LastBootUpTime FROM Win32_OperatingSystem", presetWhereClause),
            { L"CSName", L"Caption", L"Version", L"OSArchitecture", L"LocalDateTime", L"LastBootUpTime" },
            false, wmiLocale, {}, securityConfig
        );

        bool computerContextSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Domain, PartOfDomain, UserName FROM Win32_ComputerSystem", presetWhereClause),
            { L"Domain", L"PartOfDomain", L"UserName" },
            false, wmiLocale, {}, securityConfig
        );

        success = osContextSuccess && computerContextSuccess;
    }
    else if (cmd == L"qfe") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for qfe preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT HotFixID, Description, InstalledOn, InstalledBy FROM Win32_QuickFixEngineering", presetWhereClause),
            { L"HotFixID", L"Description", L"InstalledOn", L"InstalledBy" },
            false, wmiLocale, {}, securityConfig
        );
    }
    else if (cmd == L"startup") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for startup preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, Command, Location, User FROM Win32_StartupCommand", presetWhereClause),
            { L"Name", L"Command", L"Location", L"User" },
            false,
            wmiLocale,
            { L"Name", L"Command", L"Location", L"User" },
            securityConfig
        );
    }
    else if (cmd == L"share") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for share preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, Path, Description, Type FROM Win32_Share", presetWhereClause),
            { L"Name", L"Path", L"Description", L"Type" },
            false,
            wmiLocale,
            { L"Name", L"Path", L"Description", L"Type" },
            securityConfig
        );
    }
    else if (cmd == L"baseboard") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for baseboard preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Manufacturer, Product, SerialNumber, Version FROM Win32_BaseBoard", presetWhereClause),
            { L"Manufacturer", L"Product", L"SerialNumber", L"Version" },
            false,
            wmiLocale,
            { L"Manufacturer", L"Product", L"Serial Number", L"Version" },
            securityConfig
        );
    }
    else if (cmd == L"pagefile") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for pagefile preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, InitialSize, MaximumSize FROM Win32_PageFileSetting", presetWhereClause),
            { L"Name", L"InitialSize", L"MaximumSize" },
            false,
            wmiLocale,
            { L"Name", L"Initial Size (MB)", L"Maximum Size (MB)" },
            securityConfig
        );
    }
    else if (cmd == L"port" || cmd == L"ports") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for port preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (g_outputOptions.format == OutputFormat::Table) {
            std::wcout << L"\nSerial and Logical Ports:" << std::endl;
        }
        bool serialPortsSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT DeviceID, Name, Description, Status, MaxBaudRate FROM Win32_SerialPort", presetWhereClause),
            { L"DeviceID", L"Name", L"Description", L"Status", L"MaxBaudRate" },
            false, wmiLocale, { L"Port", L"Name", L"Description", L"Status", L"Max Baud" }, securityConfig
        );

        if (g_outputOptions.format == OutputFormat::Table) {
            std::wcout << L"Physical Port Connectors:" << std::endl;
        }
        bool connectorsSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Tag, PortType, ExternalReferenceDesignator, InternalReferenceDesignator FROM Win32_PortConnector", presetWhereClause),
            { L"Tag", L"PortType", L"ExternalReferenceDesignator", L"InternalReferenceDesignator" },
            false, wmiLocale, { L"Tag", L"Port Type", L"External Ref", L"Internal Ref" }, securityConfig
        );

        success = serialPortsSuccess && connectorsSuccess;
    }
    else if (cmd == L"sys") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for sys preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        bool computerSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Manufacturer, Model, SystemType FROM Win32_ComputerSystem", presetWhereClause),
            { L"Manufacturer", L"Model", L"SystemType" },
            false,
            wmiLocale,
            { L"Manufacturer", L"Model", L"System Type" },
            securityConfig
        );

        bool biosSuccess = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT SMBIOSBIOSVersion, SerialNumber FROM Win32_BIOS", presetWhereClause),
            { L"SMBIOSBIOSVersion", L"SerialNumber" },
            false,
            wmiLocale,
            { L"BIOS Version", L"Serial Number" },
            securityConfig
        );

        success = computerSuccess && biosSuccess;
    }
    else if (cmd == L"process") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for process preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT ProcessId, Name, WorkingSetSize FROM Win32_Process", presetWhereClause),
            { L"ProcessId", L"Name", L"WorkingSetSize" },
            true, wmiLocale, {}, securityConfig
        );
    } 
    else if (cmd == L"service") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for service preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, DisplayName, State, StartMode FROM Win32_Service WHERE State='Running'", presetWhereClause),
            { L"Name", L"DisplayName", L"State", L"StartMode" },
            true, wmiLocale, {}, securityConfig
        );
    } 
    else if (cmd == L"ntevent" || cmd == L"event") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for ntevent preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Logfile, EventCode, Type, SourceName, TimeGenerated FROM Win32_NTLogEvent WHERE Logfile='System' AND (Type='Error' OR Type='Warning')", presetWhereClause),
            { L"Logfile", L"EventCode", L"Type", L"SourceName", L"TimeGenerated" },
            false, wmiLocale, {}, securityConfig
        );
    }
    else if (cmd == L"apps") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport == TransportMode::DcomRpc) {
            success = PrintInstalledAppsAggregated(targetConfig, wmiLocale, securityConfig);
        } else {
            std::wstring appsError;
            success = PrintInstalledAppsViaPowerShellTransport(targetConfig, 0, appsError);
            if (!success && !appsError.empty()) {
                std::wcerr << L"[!] " << appsError << std::endl;
            }
        }
    }
    else if (cmd == L"apps32" || cmd == L"win32apps") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport == TransportMode::DcomRpc) {
            success = PrintInstalledAppsByArchitecture(targetConfig, true, wmiLocale, securityConfig);
        } else {
            std::wstring appsError;
            success = PrintInstalledAppsViaPowerShellTransport(targetConfig, 32, appsError);
            if (!success && !appsError.empty()) {
                std::wcerr << L"[!] " << appsError << std::endl;
            }
        }
    }
    else if (cmd == L"apps64" || cmd == L"win64apps") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport == TransportMode::DcomRpc) {
            success = PrintInstalledAppsByArchitecture(targetConfig, false, wmiLocale, securityConfig);
        } else {
            std::wstring appsError;
            success = PrintInstalledAppsViaPowerShellTransport(targetConfig, 64, appsError);
            if (!success && !appsError.empty()) {
                std::wcerr << L"[!] " << appsError << std::endl;
            }
        }
    }
    else if (cmd == L"devices" || cmd == L"device") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for devices preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, PNPClass, Manufacturer, Status FROM Win32_PnPEntity", presetWhereClause),
            { L"Name", L"PNPClass", L"Manufacturer", L"Status" },
            false,
            wmiLocale,
            { L"Name", L"Class", L"Manufacturer", L"Status" },
            securityConfig
        );
    }
    else if (cmd == L"threads" || cmd == L"thread") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset. Use path/query for custom filtering." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport != TransportMode::DcomRpc) {
            success = ExecuteWMIQuery(
                targetConfig,
                L"SELECT Threads FROM Win32_PerfFormattedData_PerfOS_System",
                { L"Threads" },
                false,
                wmiLocale,
                { L"Running Threads" },
                securityConfig
            );
        } else {
            success = ExecuteThreadsInfoQuery(targetConfig, wmiLocale, securityConfig);
        }
    }
    else if (cmd == L"users") {
        if (presetArgTailIndex < argc) {
            std::wcerr << L"[!] Unexpected trailing arguments for users preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        success = ExecuteWMIQuery(
            targetConfig,
            BuildWqlWithOptionalWhere(L"SELECT Name, FullName, Disabled, LocalAccount, PasswordRequired FROM Win32_UserAccount WHERE LocalAccount=TRUE", presetWhereClause),
            { L"Name", L"FullName", L"Disabled", L"LocalAccount", L"PasswordRequired" },
            false, wmiLocale, {}, securityConfig
        );
    }
    else if (cmd == L"sshcheck") {
        if (!presetWhereClause.empty()) {
            std::wcerr << L"[!] where is not supported for this preset." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
        if (targetConfig.transport != TransportMode::Ssh) {
            std::wcerr << L"[!] sshcheck requires --transport ssh." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }

        std::wstring details;
        if (!RunSshConnectivityPreflight(targetConfig, details)) {
            std::wcerr << L"[!] SSH connectivity preflight failed: " << NormalizeSshErrorText(details) << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_WMI_FAILURE;
        }

        std::vector<std::pair<std::wstring, std::wstring>> capabilityRows;
        std::wstring probeError;
        bool probeOk = ProbeSshCapabilities(targetConfig, capabilityRows, probeError);

        Table table;
        if (g_outputOptions.format == OutputFormat::Table) {
            table.headers = { L"Property", L"Value" };
            table.rows.push_back({ L"SSH Connectivity", L"Connected" });
            table.rows.push_back({ L"Node", targetConfig.node });
            if (!targetConfig.user.empty()) {
                table.rows.push_back({ L"User", targetConfig.user });
            }
            if (probeOk) {
                for (const auto& row : capabilityRows) {
                    table.rows.push_back({ row.first, row.second });
                }
            } else if (!probeError.empty()) {
                table.rows.push_back({ L"Capability Probe", NormalizeSshErrorText(probeError) });
            }
        } else {
            table.headers = { L"SSH Connectivity", L"Node", L"User" };
            table.rows.push_back({ L"Connected", targetConfig.node, targetConfig.user.empty() ? L"(default)" : targetConfig.user });
            if (probeOk) {
                for (const auto& row : capabilityRows) {
                    table.rows.push_back({ row.first, row.second });
                }
            }
        }
        table.Print();
        success = true;
    }
    else if (cmd == L"path" && argIndex + 1 < argc) {
        std::wstring targetClass = argv[argIndex + 1];
        std::wstring whereClause = L"";
        int nextIndex = argIndex + 2;

        if (nextIndex < argc && _wcsicmp(argv[nextIndex], L"where") == 0 && nextIndex + 1 < argc) {
            whereClause = argv[nextIndex + 1];
            nextIndex += 2;
        }

        if (nextIndex < argc) {
            std::wstring verb = argv[nextIndex];
            std::transform(verb.begin(), verb.end(), verb.begin(), towlower);

            if (verb == L"get" && nextIndex + 1 < argc) {
                std::vector<std::wstring> props = SplitCommaTrimmed(argv[nextIndex + 1]);

                std::wstring wql = L"SELECT " + (std::wstring)argv[nextIndex + 1] + L" FROM " + targetClass;
                if (!whereClause.empty()) wql += L" WHERE " + whereClause;
                success = ExecuteWMIQuery(targetConfig, wql, props, false, wmiLocale, {}, securityConfig);
            } else if (verb == L"get") {
                std::wcerr << L"[!] Missing property list for path get." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            } else if (verb == L"call" && nextIndex + 1 < argc) {
                std::wstring methodName = argv[nextIndex + 1];
                std::map<std::wstring, std::wstring> args;
                std::wstring parseError;
                if (!ParseNamedCallArguments(nextIndex + 2, argc, argv, args, parseError)) {
                    std::wcerr << L"[!] " << parseError << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
                success = ExecuteWmiMethodCall(targetConfig, targetClass, methodName, args, wmiLocale, securityConfig);
            } else if (verb == L"call") {
                std::wcerr << L"[!] Missing method name for path call." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            } else if (verb == L"delete") {
                if (targetConfig.transport == TransportMode::Ssh) {
                    std::wstring deleteOutput;
                    std::wstring deleteError;
                    success = SshPowerShellBackend::ExecuteDeleteInstance(targetConfig, targetClass, deleteOutput, deleteError);
                    if (!success && !deleteError.empty()) {
                        std::wcerr << L"[!] " << NormalizeSshErrorText(deleteError) << std::endl;
                    } else if (success && !deleteOutput.empty()) {
                        std::wcout << deleteOutput << std::endl;
                    }
                } else {
                    success = ExecuteWmiDelete(targetConfig, targetClass, wmiLocale, securityConfig);
                }
            } else if (verb == L"set" && nextIndex + 1 < argc) {
                std::wstring pair = argv[nextIndex + 1];
                size_t eqPos = pair.find(L'=');
                if (eqPos != std::wstring::npos) {
                    if (targetConfig.transport == TransportMode::Ssh) {
                        std::wstring setOutput;
                        std::wstring setError;
                        success = SshPowerShellBackend::ExecuteSetProperty(
                            targetConfig,
                            targetClass,
                            pair.substr(0, eqPos),
                            pair.substr(eqPos + 1),
                            setOutput,
                            setError
                        );
                        if (!success && !setError.empty()) {
                            std::wcerr << L"[!] " << NormalizeSshErrorText(setError) << std::endl;
                        } else if (success && !setOutput.empty()) {
                            std::wcout << setOutput << std::endl;
                        }
                    } else {
                        success = ExecuteWmiSetProperty(targetConfig, targetClass, pair.substr(0, eqPos), pair.substr(eqPos + 1), wmiLocale, securityConfig);
                    }
                } else {
                    std::wcerr << L"[!] Invalid set syntax. Expected <PropName>=<Value>." << std::endl;
                    if (coInitialized) CoUninitialize();
                    return EXIT_BAD_ARGS;
                }
            } else if (verb == L"set") {
                std::wcerr << L"[!] Missing property assignment for path set. Expected <PropName>=<Value>." << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            } else {
                std::wcerr << L"[!] Unsupported or malformed path verb: " << verb << std::endl;
                if (coInitialized) CoUninitialize();
                return EXIT_BAD_ARGS;
            }
        } else {
            std::wcerr << L"[!] Missing verb (get, call, set, delete) for path command." << std::endl;
            if (coInitialized) CoUninitialize();
            return EXIT_BAD_ARGS;
        }
    }
    else if (cmd == L"query" && argc >= argIndex + 3) {
        std::wstring query = argv[argIndex + 1];
        std::vector<std::wstring> props;
        for (int i = argIndex + 2; i < argc; ++i) {
            props.push_back(argv[i]);
        }
        success = ExecuteWMIQuery(targetConfig, query, props, false, wmiLocale, {}, securityConfig);
    } 
    else if (cmd == L"query") {
        std::wcerr << L"[!] Missing query text or property list for query command." << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_BAD_ARGS;
    }
    else {
        std::wcerr << L"[!] Unknown command: " << cmd << std::endl;
        if (coInitialized) CoUninitialize();
        return EXIT_UNKNOWN_COMMAND;
    }

    if (coInitialized) CoUninitialize();

    if (timingEnabled) {
        const auto commandEnd = std::chrono::steady_clock::now();
        double elapsedMs = ElapsedMilliseconds(commandStart, commandEnd);
        std::wcerr << L"[timing] Command '" << cmd << L"' completed in "
                   << std::fixed << std::setprecision(3) << elapsedMs << L" ms" << std::endl;
    }

    if (!success) {
        if (g_lastNoData) return EXIT_NO_DATA;
        return EXIT_WMI_FAILURE;
    }
    if (g_lastNoData) return EXIT_NO_DATA;
    return EXIT_OK;
}
