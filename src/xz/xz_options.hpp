#pragma once

#include "backend_config.hpp"
#include "xz.hpp"

class XzOptions {
public:
    std::vector<std::wstring> forwardedArgs;

    static void printHelp();

    static void printVersion(const BackendConfig& backend);

    static bool parse(int argc, wchar_t* argv[], XzOptions& opts, bool& showHelp, bool& showVersion);
};
