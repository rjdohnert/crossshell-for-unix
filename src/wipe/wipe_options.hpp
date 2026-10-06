#pragma once

#include "wipe.hpp"

struct WipeOptions {
    bool force = false;
    bool recursive = false;
    bool verbose = false;
    bool interactive = false;
    bool quick_mode = false;
    bool dod_mode = false;
    int passes = 4;
    std::vector<std::string> targets;
};
