#pragma once

#include "purge.hpp"

class PurgeOptions {
public:
    int keepCount = 1;
    bool log = false;
    bool confirm = false;
    bool erase = false;
    bool grandTotal = false;
    bool help = false;
    std::string helpTopic = "";
    std::wstring excludePattern = L"";
    std::wstring outputSpec = L"";
    std::wstring fileSpec = L"";

    void Parse(int argc, char* argv[]);
};
