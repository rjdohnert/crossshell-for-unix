#include "system_info.hpp"

wstring SystemInfo::GetHostNameString() {
        wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameW(buf, &size)) {
            return wstring(buf);
        }
        return L"localhost";
    }

wstring SystemInfo::ToUpper(wstring str) {
        transform(str.begin(), str.end(), str.begin(), ::towupper);
        return str;
    }
