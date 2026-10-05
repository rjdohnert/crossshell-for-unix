class ProcessLauncher {
public:
    static std::wstring QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) {
            return L"\"\"";
        }
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
            return arg;
        }

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                backslashes++;
            } else if (c == L'"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'"');
        return quoted;
    }

    static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
        switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            return TRUE;
        default:
            return FALSE;
        }
    }

    static int ExecuteCommand(int argc, wchar_t* argv[], const ExecOptions& options, DWORD& outExitCode) {
        if (options.commandIndex >= argc) {
            return 0;
        }

        std::wstring command = argv[options.commandIndex];
        std::wstring arg0 = command;
        if (!options.altArg0.empty()) {
            arg0 = options.altArg0;
        }
        if (options.loginShell) {
            arg0 = L"-" + arg0;
        }

        std::wstring cmdline = QuoteArgument(arg0);
        for (int i = options.commandIndex + 1; i < argc; ++i) {
            cmdline += L" " + QuoteArgument(argv[i]);
        }

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {};
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

        wchar_t empty_env[] = L"\0\0";
        LPVOID pEnv = options.cleanEnv ? static_cast<LPVOID>(empty_env) : NULL;

        BOOL success = CreateProcessW(
            NULL,
            &cmdline[0],
            NULL,
            NULL,
            TRUE,
            CREATE_UNICODE_ENVIRONMENT,
            pEnv,
            NULL,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                std::wcerr << L"exec: " << command << L": command not found\n";
                return 127;
            } else {
                std::wcerr << L"exec: " << command << L": failed to execute (Error " << err << L")\n";
                return 126;
            }
        }

        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        GetExitCodeProcess(hProcess.Get(), &outExitCode);

        return 0;
    }
};

class ExecReporter {
public:
    static void ReportStatus(OutputFormat format, const std::wstring& pipeCommand, DWORD exitCode) {
        if (format == OutputFormat::None && pipeCommand.empty()) {
            return;
        }

        std::wstring text;
        if (format == OutputFormat::Json) {
            text = L"{\"status\":\"completed\",\"exit_code\":" + std::to_wstring(exitCode) + L"}\n";
        } else if (format == OutputFormat::Csv) {
            text = L"status,exit_code\ncompleted," + std::to_wstring(exitCode) + L"\n";
        } else if (format == OutputFormat::Table) {
            text = L"STATUS\tEXIT_CODE\ncompleted\t" + std::to_wstring(exitCode) + L"\n";
        } else {
            text = L"completed: " + std::to_wstring(exitCode) + L"\n";
        }

        NativePipeConfigurator::PipeOutput(pipeCommand, text);
    }
};
