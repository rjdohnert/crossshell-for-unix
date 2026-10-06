#pragma once

#include "vmstat.hpp"

struct SystemSample {
    std::chrono::steady_clock::time_point timestamp;
    bool ntPerfValid = false;
    SYSTEM_PERFORMANCE_INFORMATION_FULL perfInfo{};
    PERFORMANCE_INFORMATION psApiPerf{};
    ULARGE_INTEGER idleTime{};
    ULARGE_INTEGER kernelTime{};
    ULARGE_INTEGER userTime{};
};
