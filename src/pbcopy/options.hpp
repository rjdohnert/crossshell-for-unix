#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pbcopy.hpp"

struct PbcopyOptions {
    std::wstring pasteboard = L"general";
};

class OptionParser {
public:
    static void PrintUsage();
    static void PrintVersion();
    bool Parse(int argc, wchar_t* argv[], PbcopyOptions& opts, bool& exitEarly) const;
};

#endif // OPTIONS_HPP
