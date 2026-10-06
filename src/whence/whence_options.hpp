#pragma once

#include "whence.hpp"

class WhenceOptions {
public:
    bool verbose{false};
    bool showAll{false};
    bool pathSearchOnly{false};
    std::vector<std::wstring> targets;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp(const std::wstring& progName);

    static bool parse(int argc, wchar_t* argv[], WhenceOptions& opts);
};
