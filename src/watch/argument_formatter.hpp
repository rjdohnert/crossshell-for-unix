#pragma once

#include "watch.hpp"

class ArgumentFormatter {
public:
    static std::wstring QuoteArgument(const std::wstring& arg);

    static std::wstring JoinCommandArgs(const std::vector<std::wstring>& args);

    static bool ParseNumber(const std::wstring& text, double& outValue);
};
