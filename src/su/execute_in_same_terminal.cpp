#include "execute_in_same_terminal.hpp"
#include "system_error_message.hpp"

DWORD ExecuteInSameTerminal(
    const std::wstring& username,
    const std::wstring& domain,
    const std::wstring& password,
    const std::wstring& commandLine) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    wchar_t cmdBuf[32768];
    wcscpy_s(cmdBuf, commandLine.c_str());

    // Launch the target user process via Windows Terminal.
    BOOL ok = CreateProcessWithLogonW(
        username.c_str(),
        domain.c_str(),
        password.c_str(),
        LOGON_WITH_PROFILE,
        NULL,
        cmdBuf,
        CREATE_UNICODE_ENVIRONMENT,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_LOGON_FAILURE) {
            std::wcerr << L"su: Authentication failure\n";
        } else if (err == ERROR_ACCOUNT_DISABLED) {
            std::wcerr << L"su: Account disabled\n";
        } else {
            std::wcerr << L"su: " << GetSystemErrorMessage(err) << L"\n";
        }
        return err;
    }

    // Block calling process and wait for subshell to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
}
