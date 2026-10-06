#pragma once

#include "zgrep.hpp"

struct ZgrepOptions {
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Human;
    std::wstring pipeCommand;
    std::vector<std::wstring> rawArgs;
};
