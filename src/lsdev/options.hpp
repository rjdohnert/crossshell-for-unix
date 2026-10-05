#pragma once

#include "lsdev.hpp"

struct CommandLineOptions {
    bool customizedMode = true;   // -C (default)
    bool predefinedMode = false;  // -P
    bool showHeaders = false;     // -H
    bool showHelp = false;        // -h, --help

    std::string filterClass;      // -c <Class>
    std::string filterSubclass;   // -s <Subclass>
    std::string filterType;       // -t <Type>
    std::string filterName;       // -l <Name>
    std::string filterState;      // -S <State>
    std::string customFormat;     // -F <Format>
    std::string listColumn;       // -r <ColumnName>
};

class ArgumentParser {
public:
    CommandLineOptions Parse(int argc, char* argv[]) const;
};
