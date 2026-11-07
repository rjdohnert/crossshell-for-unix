#pragma once

#include <string>
#include <vector>

struct CommandOptions {
    std::vector<std::wstring> groupNames;
    bool force = false;
    bool quiet = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
    bool readFromStdin = false;
};

struct GroupDelResult {
    unsigned long status = 0;
    bool deleted = false;
    bool notFound = false;
};
