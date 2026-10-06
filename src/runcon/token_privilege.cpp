#include "token_privilege.hpp"

bool TokenPrivilegeGuard::enablePrivilege(LPCWSTR lpszPrivilege) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValueW(NULL, lpszPrivilege, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) &&
                  (GetLastError() == ERROR_SUCCESS);

    CloseHandle(hToken);
    return result;
}

std::wstring IntegrityLevelMapper::mapLevelToSid(const std::wstring& levelStr) {
    std::wstring l = levelStr;
    for (auto& c : l) c = static_cast<wchar_t>(std::towlower(c));

    if (l == L"untrusted" || l == L"0") return L"S-1-16-0";
    if (l == L"low" || l == L"l" || l == L"4096") return L"S-1-16-4096";
    if (l == L"medium" || l == L"m" || l == L"8192") return L"S-1-16-8192";
    if (l == L"high" || l == L"h" || l == L"12288") return L"S-1-16-12288";
    if (l == L"system" || l == L"s" || l == L"16384") return L"S-1-16-16384";

    if (l.rfind(L"s-1-16-", 0) == 0) {
        return levelStr;
    }

    return L"";
}
