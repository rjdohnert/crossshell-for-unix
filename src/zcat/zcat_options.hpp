#pragma once

#include "zcat.hpp"

struct ZcatOptions {
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Human;
    std::wstring pipeCommand;
    std::vector<std::wstring> files;
};
