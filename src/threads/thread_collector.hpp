#pragma once

#include "process_thread_info.hpp"
#include "threads.hpp"

class ThreadCollector {
public:
    static std::vector<ProcessThreadInfo> collect();
};
