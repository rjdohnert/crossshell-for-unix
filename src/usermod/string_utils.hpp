#pragma once

#include "usermod.hpp"

class StringUtils {
public:
    static std::wstring ToWide(const std::string& str);

    static std::string ToUtf8(const std::wstring& wstr);

    static std::string Trim(std::string str);

    static std::vector<std::string> Split(const std::string& s, char delimiter);
};
