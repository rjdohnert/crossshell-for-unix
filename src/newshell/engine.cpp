#include "engine.hpp"

bool TerminalLauncher::IsWindowsTerminalAvailable() {
    DWORD bufferLen = SearchPathW(nullptr, L"wt.exe", nullptr, 0, nullptr, nullptr);
    return bufferLen > 0;
}

bool TerminalLauncher::Launch(const std::wstring& targetExe,
                              const std::wstring& targetArgs,
                              const std::wstring& workingDir,
                              bool runAsAdmin) {
    SHELLEXECUTEINFOW sei = { sizeof(SHELLEXECUTEINFOW) };
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    sei.lpVerb = runAsAdmin ? L"runas" : L"open";
    sei.lpFile = targetExe.c_str();
    sei.lpParameters = targetArgs.empty() ? nullptr : targetArgs.c_str();
    sei.lpDirectory = workingDir.empty() ? nullptr : workingDir.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            std::wcerr << L"[newshell] Error: Elevation request canceled by user.\n";
        } else {
            std::wcerr << L"[newshell] Error: Failed to launch process. Windows error code: " << error << L"\n";
        }
        return false;
    }
    return true;
}
