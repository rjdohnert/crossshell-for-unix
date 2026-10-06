#include "buffer_relay_engine.hpp"
#include "stdbuf_options.hpp"
#include "stdbuf_process_launcher.hpp"

int StdbufProcessLauncher::launch(const StdbufOptions& opts) {
        std::wstring cmdline;
        for (size_t k = 0; k < opts.commandArgs.size(); ++k) {
            if (k > 0) cmdline += L" ";
            if (opts.commandArgs[k].find(L' ') != std::wstring::npos) {
                cmdline += L"\"" + opts.commandArgs[k] + L"\"";
            } else {
                cmdline += opts.commandArgs[k];
            }
        }

        HANDLE hOutRead = NULL, hOutWrite = NULL;
        HANDLE hErrRead = NULL, hErrWrite = NULL;

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
        SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

        CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
        SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hOutWrite;
        si.hStdError = hErrWrite;

        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        BOOL created = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        CloseHandle(hOutWrite);
        CloseHandle(hErrWrite);

        if (!created) {
            std::wcerr << L"stdbuf: failed to run " << opts.commandArgs[0] << L"\n";
            CloseHandle(hOutRead);
            CloseHandle(hErrRead);
            return 1;
        }

        std::thread outThread(BufferRelayEngine::relayOutput, hOutRead, GetStdHandle(STD_OUTPUT_HANDLE), opts.outConfig);
        std::thread errThread(BufferRelayEngine::relayOutput, hErrRead, GetStdHandle(STD_ERROR_HANDLE), opts.errConfig);

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        outThread.join();
        errThread.join();

        CloseHandle(hOutRead);
        CloseHandle(hErrRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exitCode);
    }
