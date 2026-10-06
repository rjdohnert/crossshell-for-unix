#pragma once

#include "top.hpp"

enum class SortMode { CPU, MEMORY, PID, TIME };

struct ProcessRecord {
    DWORD pid = 0;
    std::string user = "SYSTEM";
    int pri = 0;
    int nice = 0;
    uint64_t sizeKb = 0;
    uint64_t resKb = 0;
    std::string state = "sleep";
    uint64_t cpuTimeMs = 0;
    double cpuPct = 0.0;
    std::string command;
};
