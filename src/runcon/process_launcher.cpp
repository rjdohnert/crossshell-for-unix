#include "process_launcher.hpp"

std::wstring IntegrityProcessLauncher::buildCommandLine(const std::vector<std::wstring>& args) {
    std::wstring cmdLine;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdLine += L" ";
        std::wstring arg = args[i];
        if (arg.find(L' ') != std::wstring::npos || arg.find(L'\t') != std::wstring::npos || arg.empty()) {
            cmdLine += L"\"";
            for (wchar_t c : arg) {
                if (c == L'\"') cmdLine += L"\\\"";
                else cmdLine += c;
            }
            cmdLine += L"\"";
        } else {
            cmdLine += arg;
        }
    }
    return cmdLine;
}

int IntegrityProcessLauncher::launch(const std::wstring& stringSid, const std::vector<std::wstring>& args) {
    HANDLE hToken = NULL;
    HANDLE hNewToken = NULL;
    PSID pIntegritySid = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, &hToken)) {
        std::wcerr << L"runcon: Failed to open process token (Error: " << GetLastError() << L")\n";
        return 1;
    }

    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hNewToken)) {
        std::wcerr << L"runcon: Failed to duplicate token (Error: " << GetLastError() << L")\n";
        CloseHandle(hToken);
        return 1;
    }
    CloseHandle(hToken);

    if (!ConvertStringSidToSidW(stringSid.c_str(), &pIntegritySid)) {
        std::wcerr << L"runcon: Invalid SID format or integrity string: " << stringSid << L"\n";
        CloseHandle(hNewToken);
        return 1;
    }

    TOKEN_MANDATORY_LABEL tml = { 0 };
    tml.Label.Attributes = SE_GROUP_INTEGRITY;
    tml.Label.Sid = pIntegritySid;

    if (!SetTokenInformation(hNewToken, TokenIntegrityLevel, &tml, sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(pIntegritySid))) {
        std::wcerr << L"runcon: Failed to set integrity level on token (Error: " << GetLastError() << L")\n";
        LocalFree(pIntegritySid);
        CloseHandle(hNewToken);
        return 1;
    }

    LocalFree(pIntegritySid);

    std::wstring commandLine = buildCommandLine(args);
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };

    std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessAsUserW(
        hNewToken,
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        std::wcerr << L"runcon: Failed to execute process under selected security context (Error: " << GetLastError() << L")\n";
        CloseHandle(hNewToken);
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hNewToken);

    return static_cast<int>(exitCode);
}
