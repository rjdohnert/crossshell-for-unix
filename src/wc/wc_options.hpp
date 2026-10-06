#pragma once

#include "counters.hpp"
#include "wc.hpp"

struct WcOptions {
    bool opt_bytes = false;
    bool opt_chars = false;
    bool opt_lines = false;
    bool opt_words = false;
    bool opt_max_len = false;
    bool implicit_stdin = false;
    OutputFormat format = OutputFormat::Human;
    std::string pipe_command;
    std::vector<std::string> files;
};
