#pragma once

#include "system_details.hpp"
#include "uname.hpp"

class NtKernelInfoProvider {
private:
    static std::wstring queryRegString(HKEY hKey, const wchar_t* valueName);

    static DWORD queryRegDword(HKEY hKey, const wchar_t* valueName, DWORD defaultVal = 0);

    static std::wstring detectEdition(DWORD major, DWORD minor, DWORD build, bool isServer, const std::wstring& regProductName);

public:
    static SystemDetails collect();
};
