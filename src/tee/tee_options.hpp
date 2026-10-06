#pragma once

#include "tee.hpp"

struct TeeOptions {
    bool append = false;
    bool ignore_interrupts = false;
    int output_format = 0;
    std::wstring pipe_command;
    std::vector<std::wstring> file_paths;
};
