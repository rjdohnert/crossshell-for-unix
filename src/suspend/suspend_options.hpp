#pragma once

#include "suspend.hpp"

struct SuspendOptions {
    bool resume = false;
    bool show_help = false;
    bool show_version = false;
    bool parse_error = false;
    bool safe_mode = false;
    bool enable_debug_priv = false;
    bool use_ntdll = true;
    std::vector<DWORD> pids;
    int output_format = 0;
    std::wstring pipe_command;
};
