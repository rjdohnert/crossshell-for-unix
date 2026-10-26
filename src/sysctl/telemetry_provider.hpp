#pragma once

#include "mib_tree.hpp"

struct PageFileUsage {
    size_t totalPages = 0;
    size_t usedPages = 0;
};

class WindowsTelemetryFactory {
public:
    static void Populate(MibTree& tree);

private:
    static MEMORYSTATUSEX GetMemoryStatus();
    static PageFileUsage GetPageFileUsage();
    static DWORD GetPageSize();
    static DWORD GetCpuCount();
    static std::string GetOsRelease();
};
