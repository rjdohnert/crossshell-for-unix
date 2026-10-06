#pragma once

#include "touch.hpp"

struct TouchOptions {
    bool change_access = false;     // -a
    bool no_create = false;         // -c
    bool change_mod = false;        // -m
    std::wstring ref_file = L"";    // -r
    std::wstring time_str = L"";    // -t
    std::wstring date_str = L"";    // --date
    int output_format = 0;
    std::wstring pipe_command;
    std::vector<std::wstring> targets;
};
