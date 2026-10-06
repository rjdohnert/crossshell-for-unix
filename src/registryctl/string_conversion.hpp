#pragma once

#include "registryctl.hpp"

namespace Utils {
    std::wstring ToWString(const std::string& str);

    std::string ToString(const std::wstring& wstr);

    std::string ToUpper(std::string s);

    std::string FormatWin32Error(DWORD errorCode);

    std::string BytesToHex(const std::vector<uint8_t>& bytes);

    std::vector<uint8_t> HexToBytes(const std::string& hex);
}
