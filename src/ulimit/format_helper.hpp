#pragma once

#include "ulimit.hpp"

class FormatHelper {
public:
    static std::wstring FormatBytesIec(ULONGLONG bytes);

    static std::wstring FormatDurationSeconds(ULONGLONG hundred_ns);

    static bool ParseUnsigned(const std::wstring& text, unsigned long long& value);

    static std::wstring QuoteArgument(const std::wstring& arg);

    static std::wstring BuildCommandLine(const std::vector<std::wstring>& args);
};
