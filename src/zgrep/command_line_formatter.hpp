#pragma once

#include "zgrep.hpp"

class CommandLineFormatter {
public:
    static std::wstring QuoteArgument(const std::wstring& arg);

    static std::wstring BuildCommandLine(const std::vector<std::wstring>& args);

    static std::wstring EscapeSingleQuoted(const std::wstring& s);
};
