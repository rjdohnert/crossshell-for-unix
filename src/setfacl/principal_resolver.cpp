#include "principal_resolver.hpp"

std::wstring PrincipalResolver::ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

DWORD PrincipalResolver::RightsToMask(const std::wstring& rights) {
    std::wstring normalized = ToLower(rights);
    if (normalized == L"rwx" || normalized == L"full" || normalized == L"u+rwx") {
        return FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE | WRITE_DAC | WRITE_OWNER;
    }

    DWORD mask = 0;
    if (normalized.find(L'r') != std::wstring::npos) mask |= FILE_GENERIC_READ;
    if (normalized.find(L'w') != std::wstring::npos) mask |= FILE_GENERIC_WRITE | DELETE;
    if (normalized.find(L'x') != std::wstring::npos) mask |= FILE_GENERIC_EXECUTE;
    return mask;
}

std::wstring PrincipalResolver::SidToName(PSID sid) {
    if (!sid) {
        return L"unknown";
    }

    wchar_t name[256] = {};
    wchar_t domain[256] = {};
    DWORD nameLen = 256;
    DWORD domainLen = 256;
    SID_NAME_USE sidType;
    if (LookupAccountSidW(nullptr, sid, name, &nameLen, domain, &domainLen, &sidType)) {
        if (domainLen > 0 && domain[0] != L'\0') {
            return std::wstring(domain) + L"\\" + name;
        }
        return name;
    }

    LPWSTR sidString = nullptr;
    if (ConvertSidToStringSidW(sid, &sidString)) {
        std::wstring result = sidString;
        LocalFree(sidString);
        return result;
    }
    return L"unknown";
}

PSID PrincipalResolver::NameToSid(const std::wstring& name, std::vector<BYTE>& sidBuffer, std::vector<wchar_t>& domainBuffer) {
    DWORD sidLen = 0;
    DWORD domainLen = 0;
    SID_NAME_USE sidType;
    LookupAccountNameW(nullptr, name.c_str(), nullptr, &sidLen, nullptr, &domainLen, &sidType);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || sidLen == 0) {
        return nullptr;
    }

    sidBuffer.resize(sidLen);
    domainBuffer.resize(domainLen > 0 ? domainLen : 1);
    if (!LookupAccountNameW(nullptr, name.c_str(), sidBuffer.data(), &sidLen, domainBuffer.data(), &domainLen, &sidType)) {
        return nullptr;
    }
    return reinterpret_cast<PSID>(sidBuffer.data());
}

PSID PrincipalResolver::AllocateEveryoneSid() {
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_WORLD_SID_AUTHORITY;
    PSID sid = nullptr;
    if (!AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &sid)) {
        return nullptr;
    }
    return sid;
}
