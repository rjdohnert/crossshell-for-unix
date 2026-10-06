#pragma once

#include "swlist_options.hpp"
#include "swlist.hpp"

class OptionParser {
public:
    static bool IsValidLevel(const wstring& level);

    static bool IsValidAttribute(const wstring& attribute);

    static void PrintHelp(const wchar_t* progName);

    bool Parse(int argc, wchar_t* argv[], SwlistOptions& opts) const;
};
