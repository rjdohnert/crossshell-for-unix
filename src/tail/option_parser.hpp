#pragma once

#include "tail_options.hpp"
#include "tail.hpp"

class OptionParser {
public:
    static void PrintUsage();

    static void PrintVersion();

    static bool ParseCount(const std::wstring& str, long long& count, bool& from_start);

    bool Parse(int argc, wchar_t* argv[], TailOptions& opts, bool& exitEarly) const;
};
