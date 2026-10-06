#pragma once

#include "suspend.hpp"

class StringUtils {
public:
    static std::string Utf8(const std::wstring& value);

    static std::wstring ToLowerCopy(const std::wstring& value);

    static bool ParsePid(const std::wstring& text, DWORD& pid);
};
