#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "paste.hpp"

struct PasteOptions {
    std::string delim_list = "\t";
    bool serial = false;
    bool show_help = false;
    bool show_version = false;
    std::vector<std::string> files;
};

class OptionParser {
public:
    static void PrintUsage(const char* prog_name = "paste");
    static void PrintVersion();
    bool Parse(int argc, char* argv[], PasteOptions& opts) const;
};

#endif // OPTIONS_HPP
