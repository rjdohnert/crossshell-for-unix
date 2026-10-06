#include "command_runner_engine.hpp"
#include "scoped_process_handle.hpp"

int CommandRunnerEngine::RunOnce(const std::wstring& childCmdLine) {
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {};
        std::vector<wchar_t> mutableCmd(childCmdLine.begin(), childCmdLine.end());
        mutableCmd.push_back(L'\0');

        BOOL ok = CreateProcessW(
            nullptr,
            mutableCmd.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!ok) {
            std::wcerr << L"watch: failed to execute command (error " << GetLastError() << L")\n";
            return 127;
        }

        ScopedProcessHandle hProcess(pi.hProcess);
        ScopedProcessHandle hThread(pi.hThread);

        WaitForSingleObject(hProcess.Get(), INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(hProcess.Get(), &exitCode);
        return static_cast<int>(exitCode);
    }
