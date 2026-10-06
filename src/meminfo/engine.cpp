#include "engine.hpp"

MemorySnapshot MemoryMetricsCollector::Collect() {
    MemorySnapshot snap;

    // 1. Base Physical & PageFile Availability
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    snap.totalPhysical = ms.ullTotalPhys;
    snap.availPhysical = ms.ullAvailPhys;
    snap.usedPhysical  = ms.ullTotalPhys - ms.ullAvailPhys;

    // 2. Deep Kernel & Commit Metrics
    PERFORMANCE_INFORMATION pi;
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi))) {
        uint64_t pageSize = static_cast<uint64_t>(pi.PageSize);
        snap.systemCache    = static_cast<uint64_t>(pi.SystemCache) * pageSize;
        snap.commitTotal    = static_cast<uint64_t>(pi.CommitTotal) * pageSize;
        snap.commitLimit    = static_cast<uint64_t>(pi.CommitLimit) * pageSize;
        snap.commitPeak     = static_cast<uint64_t>(pi.CommitPeak) * pageSize;
        snap.kernelPaged    = static_cast<uint64_t>(pi.KernelPaged) * pageSize;
        snap.kernelNonPaged = static_cast<uint64_t>(pi.KernelNonpaged) * pageSize;
    }

    // 3. Topology of Individual Paging Files (HP-UX swapinfo style)
    EnumPageFilesW(PageFileCallback, &snap);

    return snap;
}

BOOL CALLBACK MemoryMetricsCollector::PageFileCallback(LPVOID pContext, PENUM_PAGE_FILE_INFORMATION pInfo, LPCWSTR lpFileName) {
    auto* snap = reinterpret_cast<MemorySnapshot*>(pContext);
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    uint64_t pageSize = sysInfo.dwPageSize;

    PageDeviceInfo dev;
    dev.path = lpFileName ? lpFileName : L"Unknown";
    dev.totalBytes = static_cast<uint64_t>(pInfo->TotalSize) * pageSize;
    dev.usedBytes  = static_cast<uint64_t>(pInfo->TotalInUse) * pageSize;
    dev.peakBytes  = static_cast<uint64_t>(pInfo->PeakUsage) * pageSize;
    snap->pageFiles.push_back(dev);
    return TRUE;
}
