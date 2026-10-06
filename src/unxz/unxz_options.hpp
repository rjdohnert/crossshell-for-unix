#pragma once

#include "unxz_backend_config.hpp"
#include "unxz.hpp"

class UnxzOptions {
public:
    std::vector<std::wstring> forwardedArgs;

    static void printHelp();

    static void printVersion(const UnxzBackendConfig& backend);

    static bool parse(int argc, wchar_t* argv[], UnxzOptions& opts, bool& showHelp, bool& showVersion);
};
