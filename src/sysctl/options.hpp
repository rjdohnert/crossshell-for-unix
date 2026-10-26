#pragma once

#include <string>
#include <vector>

struct CliOptions {
    bool listAll = false;       // -a or -A
    bool listTables = false;   // -A
    bool showDesc = false;      // -d
    bool valueOnly = false;     // -n
    bool nameOnly = false;      // -N
    bool quiet = false;         // -q
    bool ignoreUnknown = false; // -i
    bool help = false;
    std::string inputFile = ""; // -f

    bool Parse(int argc, char* argv[], std::vector<std::string>& queries);
    void PrintHelp() const;
};
