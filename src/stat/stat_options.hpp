#pragma once

#include "stat.hpp"

struct ProgramOptions {
    OutputFormat format = OutputFormat::Auto;
    bool humanReadable = true;
    bool showHelp = false;
    bool showVersion = false;
    std::vector<std::wstring> targetPaths;
};
