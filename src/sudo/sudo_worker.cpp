#include "sudo_worker.hpp"

int RunWorker(const std::wstring& guid, const std::wstring& targetCmd) {
    std::wstring pipeInName  = L"\\\\.\\pipe\\sudo_in_" + guid;
    std::wstring pipeOutName = L"\\\\.\\pipe\\sudo_out_" + guid;
    std::wstring pipeErrName = L"\\\\.\\pipe\\sudo_err_" + guid;
    std::wstring pipeExitName= L"\\\\.\\pipe\\sudo_exit_" + guid;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE }; // Enable handle inheritance

    // Connect to client pipes
    HANDLE hPipeIn = CreateFileW(pipeInName.c_str(), GENERIC_READ, 0, &sa, OPEN_EXISTING, 0, NULL);
    HANDLE hPipeOut = CreateFileW(pipeOutName.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
    HANDLE hPipeErr = CreateFileW(pipeErrName.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
    HANDLE hPipeExit = CreateFileW(pipeExitName.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipeIn == INVALID_HANDLE_VALUE || hPipeOut == INVALID_HANDLE_VALUE || 
        hPipeErr == INVALID_HANDLE_VALUE || hPipeExit == INVALID_HANDLE_VALUE) {
        return 1;
    }

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hPipeIn;
    si.hStdOutput = hPipeOut;
    si.hStdError = hPipeErr;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuffer(targetCmd.begin(), targetCmd.end());
    cmdBuffer.push_back(L'\0');

    // Launch child process silently inside the elevated worker
    if (CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        DWORD written = 0;
        WriteFile(hPipeExit, &exitCode, sizeof(exitCode), &written, NULL);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        DWORD exitCode = GetLastError();
        DWORD written = 0;
        WriteFile(hPipeExit, &exitCode, sizeof(exitCode), &written, NULL);
    }

    CloseHandle(hPipeIn);
    CloseHandle(hPipeOut);
    CloseHandle(hPipeErr);
    CloseHandle(hPipeExit);
    return 0;
}
