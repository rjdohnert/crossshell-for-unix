#pragma once

#include "top.hpp"

enum class SortMode { CPU, MEMORY, PID, TIME, NAME };

struct ProcessRecord {
    DWORD pid = 0;
    std::string user = "-";
    int pri = 0;
    int nice = 0;
    uint64_t sizeKb = 0;
    uint64_t resKb = 0;
    std::string activity = "idle";
    uint64_t cpuTimeMs = 0;
    double cpuPct = 0.0;
    std::string command;
};
