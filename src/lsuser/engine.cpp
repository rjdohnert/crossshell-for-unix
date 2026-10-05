#include "engine.hpp"
#include <sstream>
#include <algorithm>
#include <cwctype>

std::wstring UserAccountEnumerator::ToWide(const std::string& input) {
    if (input.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (sizeNeeded <= 1) return L"";
    std::wstring result(static_cast<size_t>(sizeNeeded - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, &result[0], sizeNeeded);
    return result;
}

std::wstring UserAccountEnumerator::ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return std::towlower(ch);
    });
    return value;
}

std::wstring UserAccountEnumerator::NormalizeText(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return L"(none)";
    }
    return value;
}

std::wstring UserAccountEnumerator::NormalizeText(const std::wstring& value) {
    if (value.empty()) {
        return L"(none)";
    }
    return value;
}

std::wstring UserAccountEnumerator::ResolveFullName(const std::wstring& username, bool disabled) {
    if (disabled) return L"(none)";
    LPUSER_INFO_3 userInfo = nullptr;
    NET_API_STATUS status = NetUserGetInfo(nullptr, const_cast<LPWSTR>(username.c_str()), 3, reinterpret_cast<LPBYTE*>(&userInfo));
    if (status == NERR_Success && userInfo != nullptr) {
        ScopedNetApiBuffer<USER_INFO_3> buf(userInfo);
        std::wstring fullName = NormalizeText(userInfo->usri3_full_name);
        if (fullName != L"(none)") return fullName;
    }
    return L"(none)";
}

std::wstring UserAccountEnumerator::ResolveHomeDirectory(const std::wstring& username, bool disabled) {
    if (disabled) return L"(none)";
    LPUSER_INFO_3 userInfo = nullptr;
    NET_API_STATUS status = NetUserGetInfo(nullptr, const_cast<LPWSTR>(username.c_str()), 3, reinterpret_cast<LPBYTE*>(&userInfo));
    if (status == NERR_Success && userInfo != nullptr) {
        ScopedNetApiBuffer<USER_INFO_3> buf(userInfo);
        std::wstring home = NormalizeText(userInfo->usri3_home_dir);
        if (home != L"(none)") return home;
    }
    return L"(none)";
}

std::wstring UserAccountEnumerator::ResolveDefaultShell() {
    wchar_t comspec[MAX_PATH] = { 0 };
    DWORD size = GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH);
    if (size == 0 || comspec[0] == L'\0') {
        return L"cmd.exe";
    }

    std::wstring shell = comspec;
    std::wstring lower = ToLower(shell);
    if (lower.find(L"powershell") != std::wstring::npos || lower.find(L"pwsh") != std::wstring::npos) {
        return L"powershell.exe";
    }
    if (lower.find(L"cmd") != std::wstring::npos) {
        return L"cmd.exe";
    }
    return shell;
}

std::wstring UserAccountEnumerator::FormatDurationFromSeconds(ULONGLONG seconds) {
    if (seconds == 0) return L"0s";

    ULONGLONG days = seconds / 86400ULL;
    seconds %= 86400ULL;
    ULONGLONG hours = seconds / 3600ULL;
    seconds %= 3600ULL;
    ULONGLONG minutes = seconds / 60ULL;
    seconds %= 60ULL;

    std::wstringstream stream;
    if (days > 0) stream << days << L"d ";
    if (hours > 0 || days > 0) stream << hours << L"h ";
    if (minutes > 0 || hours > 0 || days > 0) stream << minutes << L"m ";
    stream << seconds << L"s";
    return stream.str();
}

std::wstring UserAccountEnumerator::ResolveGroupMembership(const std::wstring& username) {
    LPLOCALGROUP_USERS_INFO_0 groups = nullptr;
    DWORD entriesRead = 0;
    DWORD totalEntries = 0;

    NET_API_STATUS status = NetUserGetLocalGroups(
        nullptr,
        const_cast<LPWSTR>(username.c_str()),
        0,
        0,
        reinterpret_cast<LPBYTE*>(&groups),
        MAX_PREFERRED_LENGTH,
        &entriesRead,
        &totalEntries);

    if (status != NERR_Success && status != ERROR_MORE_DATA) {
        return L"(none)";
    }
    ScopedNetApiBuffer<LOCALGROUP_USERS_INFO_0> buf(groups);

    std::vector<std::wstring> names;
    for (DWORD i = 0; i < entriesRead; ++i) {
        if (groups[i].lgrui0_name != nullptr) {
            names.push_back(NormalizeText(groups[i].lgrui0_name));
        }
    }

    if (names.empty()) return L"(none)";

    std::wstringstream stream;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) stream << L", ";
        stream << names[i];
    }
    return stream.str();
}

std::wstring UserAccountEnumerator::ResolveLoginDuration(const std::wstring& username) {
    DWORD sessionCount = 0;
    PLUID sessionList = nullptr;
    NTSTATUS status = LsaEnumerateLogonSessions(&sessionCount, &sessionList);
    if (status != 0 || sessionList == nullptr) {
        return L"(none)";
    }

    std::wstring result = L"(none)";
    for (DWORD i = 0; i < sessionCount; ++i) {
        PSECURITY_LOGON_SESSION_DATA sessionData = nullptr;
        NTSTATUS dataStatus = LsaGetLogonSessionData(&sessionList[i], &sessionData);
        if (dataStatus == 0 && sessionData != nullptr) {
            std::wstring sessionUser(sessionData->UserName.Buffer, sessionData->UserName.Length / sizeof(wchar_t));
            if (ToLower(sessionUser) == ToLower(username)) {
                ULARGE_INTEGER logonTime;
                logonTime.QuadPart = sessionData->LogonTime.QuadPart;

                FILETIME nowFileTime;
                GetSystemTimeAsFileTime(&nowFileTime);
                ULARGE_INTEGER now;
                now.LowPart = nowFileTime.dwLowDateTime;
                now.HighPart = nowFileTime.dwHighDateTime;

                ULONGLONG elapsed100ns = now.QuadPart > logonTime.QuadPart ? now.QuadPart - logonTime.QuadPart : 0;
                ULONGLONG seconds = elapsed100ns / 10000000ULL;
                result = (seconds == 0) ? L"0s" : FormatDurationFromSeconds(seconds);
            }
            LsaFreeReturnBuffer(sessionData);
        }
    }

    LsaFreeReturnBuffer(sessionList);
    return result;
}

std::vector<AccountRecord> UserAccountEnumerator::EnumerateLocalAccounts() {
    std::vector<AccountRecord> accounts;
    LPUSER_INFO_1 users = nullptr;
    DWORD entriesRead = 0;
    DWORD totalEntries = 0;
    DWORD resumeHandle = 0;

    NET_API_STATUS status = NetUserEnum(
        nullptr,
        1,
        FILTER_NORMAL_ACCOUNT,
        reinterpret_cast<LPBYTE*>(&users),
        MAX_PREFERRED_LENGTH,
        &entriesRead,
        &totalEntries,
        &resumeHandle);

    if (status != NERR_Success && status != ERROR_MORE_DATA) {
        return accounts;
    }
    ScopedNetApiBuffer<USER_INFO_1> buf(users);

    std::wstring defaultShell = ResolveDefaultShell();

    for (DWORD i = 0; i < entriesRead; ++i) {
        AccountRecord account;
        account.name = NormalizeText(users[i].usri1_name);
        account.fullName = ResolveFullName(account.name, account.disabled);
        account.disabled = (users[i].usri1_flags & UF_ACCOUNTDISABLE) != 0;
        account.homeDir = ResolveHomeDirectory(account.name, account.disabled);
        account.shell = defaultShell;
        account.comment = NormalizeText(users[i].usri1_comment);
        account.groups = ResolveGroupMembership(account.name);
        account.loginDuration = ResolveLoginDuration(account.name);
        account.uid = 1000 + i + 1;
        account.status = account.disabled ? L"disabled" : L"active";
        accounts.push_back(account);
    }

    return accounts;
}
