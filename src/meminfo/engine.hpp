#ifndef MEMINFO_ENGINE_HPP
#define MEMINFO_ENGINE_HPP

#include "meminfo.hpp"

class MemoryMetricsCollector {
public:
    static MemorySnapshot Collect();
private:
    static BOOL CALLBACK PageFileCallback(LPVOID pContext, PENUM_PAGE_FILE_INFORMATION pInfo, LPCWSTR lpFileName);
};

#endif // MEMINFO_ENGINE_HPP
