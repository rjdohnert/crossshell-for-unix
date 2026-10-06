#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "patch.hpp"

struct PatchOptions {
    int stripCount = 0;
    std::string patchFile = "";
    std::string outputFile = "";
    std::string backupSuffix = ".orig";
    std::string changeDir = "";
    bool reverse = false;
    bool makeBackup = false;
    bool dryRun = false;
    bool removeEmpty = false;
    std::vector<std::string> positionalArgs;
};

class OptionParser {
public:
    static void PrintHelp();
    static void PrintVersion();
    bool Parse(int argc, char* argv[], PatchOptions& opts, bool& exitEarly) const;
};

#endif // OPTIONS_HPP
