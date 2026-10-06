#pragma once

#include "ulimit.hpp"

struct ProcessSnapshot {
    SIZE_T working_set = 0;
    SIZE_T peak_working_set = 0;
    SIZE_T private_usage = 0;
    ULONGLONG user_time_100ns = 0;
    ULONGLONG kernel_time_100ns = 0;
    DWORD handle_count = 0;
    DWORD thread_count = 0;
};
