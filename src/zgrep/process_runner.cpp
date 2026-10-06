#include "command_line_formatter.hpp"
#include "process_runner.hpp"

int ProcessRunner::Run(const wchar_t* app, const std::vector<std::wstring>& args, HANDLE output ) const {
        std::wstring cmdLine = CommandLineFormatter::BuildCommandLine(args);
        std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = output == INVALID_HANDLE_VALUE ? GetStdHandle(STD_OUTPUT_HANDLE) : output;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(app, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi)) {
            return 126;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
