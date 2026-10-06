#pragma once

#include "top.hpp"

struct CpuSnapshot {
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
    uint64_t timestamp = 0;
};
