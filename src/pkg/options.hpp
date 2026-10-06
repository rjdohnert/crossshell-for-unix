#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pkg.hpp"

class PkgOptions {
public:
    std::wstring command;
    std::vector<std::wstring> arguments;

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], PkgOptions& opts, bool& showHelp, bool& showVersion);
};

#endif // OPTIONS_HPP
