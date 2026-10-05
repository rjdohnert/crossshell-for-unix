// ============================================================================
// Process Runner & Pipe Relay
// ============================================================================

class ProcessRunner {
public:
    static int execute(
        const std::vector<std::wstring>& commandAndArgs,
        const std::vector<wchar_t>& envBlock,
        const std::wstring& workingDirectory,
        bool verbose) 
    {
        if (commandAndArgs.empty()) return 0;

        std::wstring commandLine = ArgumentTokenizer::buildWindowsCommandLine(commandAndArgs);

        if (verbose) {
            std::wcerr << L"env: executing: " << commandLine << L"\n";
            if (!workingDirectory.empty()) {
                std::wcerr << L"env: working directory: " << workingDirectory << L"\n";
            }
        }

        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        // Seamless pipe inheritance: inherit the current process's standard handles
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        // Create mutable command line buffer for CreateProcessW
        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        LPCWSTR lpCurrentDir = workingDirectory.empty() ? nullptr : workingDirectory.c_str();
        LPVOID lpEnvironment = envBlock.empty() ? nullptr : (LPVOID)envBlock.data();

        DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;

        BOOL success = CreateProcessW(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            TRUE, // Inherit standard pipes and handles
            creationFlags,
            lpEnvironment,
            lpCurrentDir,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = GetLastError();
            std::wcerr << L"env: '" << commandAndArgs[0] << L"': ";
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
                std::wcerr << L"No such file or directory\n";
                return 127; // POSIX standard: 127 for not found
            } else if (err == ERROR_ACCESS_DENIED) {
                std::wcerr << L"Permission denied\n";
                return 126; // POSIX standard: 126 for non-executable
            } else {
                std::wcerr << L"Failed to create process (Error " << err << L")\n";
                return 126;
            }
        }

        // Wait for child process to finish
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exitCode);
    }
};
