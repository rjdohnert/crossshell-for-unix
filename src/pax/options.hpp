#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pax.hpp"

struct PaxOptions {
    bool mode_read = false;
    bool mode_write = false;
    bool verbose = false;
    bool keep = false;
    bool update = false;
    std::string archive_file = "";
    std::vector<Substitution> substitutions;
    std::vector<fs::path> positionals;
};

class OptionParser {
public:
    static void PrintHelp();
    static void PrintVersion();
    bool Parse(int argc, char* argv[], PaxOptions& opts, bool& exitEarly) const;
};

#endif // OPTIONS_HPP
