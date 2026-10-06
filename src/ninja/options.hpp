#ifndef NINJA_OPTIONS_HPP
#define NINJA_OPTIONS_HPP

#include "ninja.hpp"

namespace EnterpriseNinja {

struct NinjaOptions {
    std::string manifest = "build.ninja";
    int jobs = 4;
    int keep_going = 1;
    bool dry_run = false;
    bool verbose = false;
    std::vector<std::string> target_names;
    bool show_help = false;
    bool show_version = false;
    bool valid = true;
    std::string error_message;
};

class NinjaOptionsParser {
public:
    static NinjaOptions Parse(int argc, char** argv);
    static void PrintHelp();
    static void PrintVersion();
};

} // namespace EnterpriseNinja

#endif // NINJA_OPTIONS_HPP
