#pragma once

#include "resolve.hpp"

class StringEncoding {
public:
    static std::string WideToUtf8(const wchar_t* wstr);

    static std::wstring Utf8ToWide(const std::string& str);

    static std::string FormatInAddrArpa(const std::string& ipStr);

    static std::string FormatIp6Arpa(const std::string& ipStr);
};
