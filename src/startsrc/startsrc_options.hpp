#pragma once

#include "startsrc.hpp"

struct StartsrcOptions {
    std::wstring service;
    DWORD timeoutMs = 30000;
    bool help = false;
    bool version = false;
};
