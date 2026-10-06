#pragma once

#include "recycle.hpp"
#include "wide_pipe_buffer.hpp"

struct RecycleOptions {
    bool verbose = false;
    bool interactive = false;
    bool quiet = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat outputFormat = OutputFormat::Human;
    std::wstring pipeCommand;
    std::vector<std::wstring> targets;
};
