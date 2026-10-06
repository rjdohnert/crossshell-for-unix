#pragma once

#include "strace.hpp"

class ProcessTracker {
public:
    std::unordered_map<DWORD, HANDLE> processHandles;
    std::unordered_map<DWORD, HANDLE> threadHandles;
    size_t activeProcessCount = 0;

    ~ProcessTracker();
    void CleanupAll();
};
