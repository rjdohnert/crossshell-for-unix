#ifndef MVDIR_OPTIONS_HPP
#define MVDIR_OPTIONS_HPP

#include "mvdir.hpp"

struct MvdirOptions {
    bool show_help = false;
    bool show_version = false;
    fs::path source;
    fs::path destination;
};

class OptionParser {
public:
    static void PrintVersion();
    static void PrintHelp();
    bool Parse(int argc, wchar_t* argv[], MvdirOptions& opts) const;
};

#endif // MVDIR_OPTIONS_HPP
