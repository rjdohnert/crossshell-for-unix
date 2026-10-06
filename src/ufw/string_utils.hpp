#pragma once

#include "ufw.hpp"

class StringUtils {
public:
    static std::string ToLower(const std::string& text);

    static std::wstring ToWide(const std::string& str);

    static std::string ToNarrow(const std::wstring& wstr);

    static bool ParseRuleNumber(const std::string& text, int& value);

    static bool ParsePortSpec(const std::string& spec, std::string& port, LONG& protocol);
};
