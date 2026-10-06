#pragma once

#include "buffer_config.hpp"
#include "stdbuf.hpp"

class StdbufOptions {
public:
    BufferConfig inConfig;
    BufferConfig outConfig;
    BufferConfig errConfig;
    bool inSet{false};
    bool outSet{false};
    bool errSet{false};
    std::vector<std::wstring> commandArgs;

    static void printHelp();

    static void printVersion();

    static bool parse(int argc, wchar_t* argv[], StdbufOptions& opts);
};
