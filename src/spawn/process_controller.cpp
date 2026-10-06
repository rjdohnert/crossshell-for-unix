#include "process_controller.hpp"

bool ProcessController::launch(const std::wstring& cmd,
                               const SpawnOptions& opts,
                               const VmsStatusReporter& reporter,
                               DWORD& outExitCode) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    AutoHandle hInput;
    AutoHandle hOutput;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!opts.inputFile.empty()) {
        hInput = AutoHandle(::CreateFileW(
            opts.inputFile.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            &sa,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (!hInput.isValid()) {
            reporter.printStatus(L'E', L"SPAWN", L"OPENIN", L"Error opening input file: " + opts.inputFile);
            return false;
        }
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hInput.get();
    }

    if (!opts.outputFile.empty()) {
        hOutput = AutoHandle(::CreateFileW(
            opts.outputFile.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            &sa,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (!hOutput.isValid()) {
            reporter.printStatus(L'E', L"SPAWN", L"OPENOUT", L"Error opening output file: " + opts.outputFile);
            return false;
        }
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hOutput.get();
        si.hStdError = hOutput.get();
    }

    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');

    BOOL created = ::CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!created) {
        DWORD err = ::GetLastError();
        reporter.printStatus(L'F', L"SPAWN", L"CREPRC", L"Process creation failed with error " + std::to_wstring(err));
        return false;
    }

    AutoHandle hProc(pi.hProcess);
    AutoHandle hThread(pi.hThread);

    std::wstring name = opts.processName.empty() ? L"SUBPROCESS" : opts.processName;
    reporter.printStatus(L'S', L"SPAWN", L"CREATED", L"Process " + name + L" created (PID: " + std::to_wstring(pi.dwProcessId) + L")");

    if (opts.wait) {
        ::WaitForSingleObject(hProc.get(), INFINITE);
        ::GetExitCodeProcess(hProc.get(), &outExitCode);
        reporter.printStatus(L'S', L"SPAWN", L"RETURNED", L"Control returned to parent process.");
    } else {
        outExitCode = 0;
        if (opts.notify) {
            reporter.printStatus(L'I', L"SPAWN", L"NOWAIT", L"Subprocess executing in background with notification enabled.");
        }
    }

    return true;
}
