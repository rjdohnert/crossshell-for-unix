#include "command_line_formatter.hpp"
#include "process_runner.hpp"

int ProcessRunner::RunPowerShell(const std::wstring& script) const {
        std::vector<std::wstring> args = {
            L"powershell",
            L"-NoProfile",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-Command",
            script
        };

        std::wstring cmdLine = CommandLineFormatter::BuildCommandLine(args);
        std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi)) {
            return 126;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
