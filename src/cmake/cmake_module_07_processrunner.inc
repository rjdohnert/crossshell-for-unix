class ProcessRunner {
public:
    static int Execute(const std::vector<std::wstring>& args, const fs::path& workingDir) {
        if (args.empty()) return -1;

        std::optional<std::wstring> resolvedExe = StringUtil::ResolveExecutablePath(args[0]);

        // If resolved to an absolute path, quote it; otherwise use original token
        std::wstring exeToken = resolvedExe.has_value() ? *resolvedExe : args[0];
        std::wstring commandLine = StringUtil::QuoteArgument(exeToken);

        for (size_t i = 1; i < args.size(); ++i) {
            commandLine += L" ";
            commandLine += StringUtil::QuoteArgument(args[i]);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);

        HANDLE hStdIn = ::GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hStdErr = ::GetStdHandle(STD_ERROR_HANDLE);

        bool hasValidStdHandle = false;

        if (hStdIn && hStdIn != INVALID_HANDLE_VALUE) {
            if (::SetHandleInformation(hStdIn, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                si.hStdInput = hStdIn;
                hasValidStdHandle = true;
            }
        }
        if (hStdOut && hStdOut != INVALID_HANDLE_VALUE) {
            if (::SetHandleInformation(hStdOut, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                si.hStdOutput = hStdOut;
                hasValidStdHandle = true;
            }
        }
        if (hStdErr && hStdErr != INVALID_HANDLE_VALUE) {
            if (::SetHandleInformation(hStdErr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                si.hStdError = hStdErr;
                hasValidStdHandle = true;
            }
        }

        if (hasValidStdHandle) {
            si.dwFlags |= STARTF_USESTDHANDLES;
        }

        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        std::wstring workDirStr = workingDir.wstring();
        LPCWSTR lpWorkDir = workDirStr.empty() ? nullptr : workDirStr.c_str();

        // Pass lpApplicationName ONLY if resolved to an absolute path; otherwise pass nullptr to allow PATH search
        LPCWSTR lpApplicationName = resolvedExe.has_value() ? resolvedExe->c_str() : nullptr;

        BOOL success = ::CreateProcessW(
            lpApplicationName,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            hasValidStdHandle ? TRUE : FALSE,
            CREATE_SUSPENDED,
            nullptr,
            lpWorkDir,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = ::GetLastError();
            Log::Message(Log::Level::Error, "Failed to launch process: " + 
                         StringUtil::WideToUtf8(args[0]) + 
                         " (Win32 Error: " + std::to_string(err) + ")");
            return -1;
        }

        ScopedHandle hProcess(pi.hProcess);
        ScopedHandle hThread(pi.hThread);

        GlobalJobTracker::Instance().AssignProcess(hProcess.Get());
        ::ResumeThread(hThread.Get());

        DWORD waitResult = ::WaitForSingleObject(hProcess.Get(), INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            Log::Message(Log::Level::Error, "Process wait failed with error: " + std::to_string(::GetLastError()));
            return -1;
        }

        DWORD exitCode = 0;
        if (!::GetExitCodeProcess(hProcess.Get(), &exitCode)) {
            Log::Message(Log::Level::Error, "Failed to retrieve process exit code.");
            return -1;
        }

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// Full CMake Command-Line Interface
// ============================================================================
