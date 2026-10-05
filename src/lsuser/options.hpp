#pragma once

#include "lsuser.hpp"
#include <string>
#include <vector>

class LsuserOptions {
public:
    std::vector<std::string> requestedAttributes;
    std::vector<std::wstring> requestedUsers;
    LsuserFormat format = LsuserFormat::Table;
    bool showHelp = false;
    bool showVersion = false;

    static std::vector<std::string> SplitAttributes(const std::string& value);
    bool Parse(int argc, char* argv[]);
    void PrintUsage(const char* prog = "lsuser") const;
    void PrintVersion() const;
};
