#pragma once

#include "search_path_config.hpp"
#include "whereis.hpp"

class WhereisOptions {
public:
    SearchPathConfig config;
    std::vector<std::wstring> targets;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printUsage(const wchar_t* progName);

    static bool parse(int argc, wchar_t* argv[], WhereisOptions& opts);
};
