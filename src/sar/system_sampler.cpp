#include "system_sampler.hpp"
#include "report_formatter.hpp"

static inline uint64_t FileTimeToQuad(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

SystemSampler::SystemSampler() {
    GetSystemTimes(&prevIdle_, &prevKernel_, &prevUser_);

    if (pdhQuery_.isValid()) {
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &hDiskBusy_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\Current Disk Queue Length", 0, &hDiskQueue_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\Disk Transfers/sec", 0, &hDiskXfers_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\Disk Bytes/sec", 0, &hDiskBytes_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\Avg. Disk sec/Transfer", 0, &hDiskLatency_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\Memory\\Pages Input/sec", 0, &hPagesIn_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\Memory\\Pages Output/sec", 0, &hPagesOut_);

        PdhAddEnglishCounterW(pdhQuery_, L"\\System\\Processor Queue Length", 0, &hProcQueue_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\System\\Processes", 0, &hProcCount_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\System\\Threads", 0, &hThreadCount_);

        PdhCollectQueryData(pdhQuery_);
    }
    initNetworkStats();
}

void SystemSampler::initNetworkStats() {
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR) {
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto& row = table->Table[i];
            if (row.OperStatus == IfOperStatusUp && row.Type != IF_TYPE_SOFTWARE_LOOPBACK) {
                prevRxBytes_ += row.InOctets;
                prevTxBytes_ += row.OutOctets;
                prevRxPackets_ += row.InUcastPkts + row.InNUcastPkts;
                prevTxPackets_ += row.OutUcastPkts + row.OutNUcastPkts;
            }
        }
        FreeMibTable(table);
    }
}

CpuSample SystemSampler::sampleCpu() {
    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);

    uint64_t i1 = FileTimeToQuad(prevIdle_), i2 = FileTimeToQuad(idle);
    uint64_t k1 = FileTimeToQuad(prevKernel_), k2 = FileTimeToQuad(kernel);
    uint64_t u1 = FileTimeToQuad(prevUser_), u2 = FileTimeToQuad(user);

    prevIdle_ = idle; prevKernel_ = kernel; prevUser_ = user;

    uint64_t dIdle = i2 - i1;
    uint64_t dKernel = k2 - k1;
    uint64_t dUser = u2 - u1;
    uint64_t dTotal = dKernel + dUser; // Kernel includes Idle in NT API

    CpuSample s;
    if (dTotal > 0) {
        uint64_t dTrueKernel = (dKernel >= dIdle) ? (dKernel - dIdle) : 0;
        s.usr = (static_cast<double>(dUser) / dTotal) * 100.0;
        s.sys = (static_cast<double>(dTrueKernel) / dTotal) * 100.0;
        s.idle = (static_cast<double>(dIdle) / dTotal) * 100.0;
        s.wio = 0.0;
    }
    return s;
}

MemorySample SystemSampler::sampleMemory() {
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    MemorySample s;
    s.totalMemKb = ms.ullTotalPhys / 1024;
    s.freeMemKb = ms.ullAvailPhys / 1024;
    s.memUsedPct = static_cast<double>(ms.dwMemoryLoad);

    uint64_t totalSwap = ms.ullTotalPageFile / 1024;
    s.freeSwapKb = ms.ullAvailPageFile / 1024;
    if (totalSwap > 0) {
        s.swapUsedPct = ((static_cast<double>(totalSwap - s.freeSwapKb)) / totalSwap) * 100.0;
    }
    return s;
}

DiskSample SystemSampler::sampleDisk() {
    DiskSample s;
    if (!pdhQuery_.isValid()) return s;

    PdhCollectQueryData(pdhQuery_);

    PDH_FMT_COUNTERVALUE val;
    if (PdhGetFormattedCounterValue(hDiskBusy_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS)
        s.busyPct = (std::min)(val.doubleValue, 100.0);

    if (PdhGetFormattedCounterValue(hDiskQueue_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS)
        s.avgQueue = val.doubleValue;

    if (PdhGetFormattedCounterValue(hDiskXfers_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS)
        s.transfersPerSec = val.doubleValue;

    if (PdhGetFormattedCounterValue(hDiskBytes_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS)
        s.blocksPerSec = val.doubleValue / 512.0;

    if (PdhGetFormattedCounterValue(hDiskLatency_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS) {
        s.avgWaitMs = val.doubleValue * 1000.0;
        s.avgServMs = val.doubleValue * 1000.0;
    }
    return s;
}

std::vector<DiskSample> SystemSampler::sampleDiskDetail() {
    std::vector<DiskSample> results;

    DWORD counterListLength = 0;
    DWORD instanceListLength = 0;
    PDH_STATUS status = PdhEnumObjectItemsW(
        NULL,
        NULL,
        L"PhysicalDisk",
        NULL,
        &counterListLength,
        NULL,
        &instanceListLength,
        PERF_DETAIL_WIZARD,
        0);

    if (status != PDH_MORE_DATA && status != ERROR_SUCCESS) {
        return results;
    }
    if (instanceListLength == 0) {
        return results;
    }

    std::vector<wchar_t> instanceBuffer(instanceListLength + 1, L'\0');
    std::vector<wchar_t> counterBuffer(counterListLength + 1, L'\0');

    status = PdhEnumObjectItemsW(
        NULL,
        NULL,
        L"PhysicalDisk",
        counterBuffer.empty() ? NULL : counterBuffer.data(),
        &counterListLength,
        instanceBuffer.data(),
        &instanceListLength,
        PERF_DETAIL_WIZARD,
        0);

    if (status != ERROR_SUCCESS) {
        return results;
    }

    const wchar_t* ptr = instanceBuffer.data();
    while (*ptr != L'\0') {
        std::wstring instance = ptr;
        if (!instance.empty() && instance != L"_Total") {
            DiskSample sample;
            sample.device = WideToUtf8(instance);

            std::wstring busyPath = L"\\PhysicalDisk(" + instance + L")\\% Disk Time";
            std::wstring queuePath = L"\\PhysicalDisk(" + instance + L")\\Current Disk Queue Length";
            std::wstring xfersPath = L"\\PhysicalDisk(" + instance + L")\\Disk Transfers/sec";
            std::wstring bytesPath = L"\\PhysicalDisk(" + instance + L")\\Disk Bytes/sec";
            std::wstring waitPath = L"\\PhysicalDisk(" + instance + L")\\Avg. Disk sec/Transfer";

            HQUERY query = nullptr;
            HCOUNTER busy = nullptr;
            HCOUNTER queue = nullptr;
            HCOUNTER xfers = nullptr;
            HCOUNTER bytes = nullptr;
            HCOUNTER wait = nullptr;

            if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) {
                if (PdhAddEnglishCounterW(query, busyPath.c_str(), 0, &busy) == ERROR_SUCCESS &&
                    PdhAddEnglishCounterW(query, queuePath.c_str(), 0, &queue) == ERROR_SUCCESS &&
                    PdhAddEnglishCounterW(query, xfersPath.c_str(), 0, &xfers) == ERROR_SUCCESS &&
                    PdhAddEnglishCounterW(query, bytesPath.c_str(), 0, &bytes) == ERROR_SUCCESS &&
                    PdhAddEnglishCounterW(query, waitPath.c_str(), 0, &wait) == ERROR_SUCCESS) {
                    if (PdhCollectQueryData(query) == ERROR_SUCCESS) {
                        PDH_FMT_COUNTERVALUE value = {};
                        if (PdhGetFormattedCounterValue(busy, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS)
                            sample.busyPct = (std::min)(value.doubleValue, 100.0);
                        if (PdhGetFormattedCounterValue(queue, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS)
                            sample.avgQueue = value.doubleValue;
                        if (PdhGetFormattedCounterValue(xfers, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS)
                            sample.transfersPerSec = value.doubleValue;
                        if (PdhGetFormattedCounterValue(bytes, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS)
                            sample.blocksPerSec = value.doubleValue / 512.0;
                        if (PdhGetFormattedCounterValue(wait, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
                            sample.avgWaitMs = value.doubleValue * 1000.0;
                            sample.avgServMs = value.doubleValue * 1000.0;
                        }
                    }
                }
                PdhCloseQuery(query);
            }
            results.push_back(sample);
        }
        ptr += instance.size() + 1;
    }

    return results;
}

NetSample SystemSampler::sampleNetwork(double intervalSec) {
    NetSample s;
    PMIB_IF_TABLE2 table = nullptr;
    uint64_t currRxBytes = 0, currTxBytes = 0;
    uint64_t currRxPackets = 0, currTxPackets = 0;

    if (GetIfTable2(&table) == NO_ERROR) {
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto& row = table->Table[i];
            if (row.OperStatus == IfOperStatusUp && row.Type != IF_TYPE_SOFTWARE_LOOPBACK) {
                currRxBytes += row.InOctets;
                currTxBytes += row.OutOctets;
                currRxPackets += row.InUcastPkts + row.InNUcastPkts;
                currTxPackets += row.OutUcastPkts + row.OutNUcastPkts;
            }
        }
        FreeMibTable(table);
    }

    if (intervalSec > 0) {
        s.rxPckSec = static_cast<double>(currRxPackets - prevRxPackets_) / intervalSec;
        s.txPckSec = static_cast<double>(currTxPackets - prevTxPackets_) / intervalSec;
        s.rxKbSec = (static_cast<double>(currRxBytes - prevRxBytes_) / 1024.0) / intervalSec;
        s.txKbSec = (static_cast<double>(currTxBytes - prevTxBytes_) / 1024.0) / intervalSec;
    }

    prevRxBytes_ = currRxBytes; prevTxBytes_ = currTxBytes;
    prevRxPackets_ = currRxPackets; prevTxPackets_ = currTxPackets;

    return s;
}

QueueSample SystemSampler::sampleQueue() {
    QueueSample s;
    if (!pdhQuery_.isValid()) return s;

    PDH_FMT_COUNTERVALUE val;
    if (PdhGetFormattedCounterValue(hProcQueue_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS)
        s.runqSz = val.doubleValue;

    if (PdhGetFormattedCounterValue(hProcCount_, PDH_FMT_LONG, NULL, &val) == ERROR_SUCCESS)
        s.processCount = static_cast<uint32_t>(val.longValue);

    if (PdhGetFormattedCounterValue(hThreadCount_, PDH_FMT_LONG, NULL, &val) == ERROR_SUCCESS)
        s.threadCount = static_cast<uint32_t>(val.longValue);

    s.runOccPct = (s.runqSz > 0) ? 100.0 : 0.0;
    return s;
}

PagingSample SystemSampler::samplePaging() {
    PagingSample s;
    if (!pdhQuery_.isValid()) return s;
    PdhCollectQueryData(pdhQuery_);

    PDH_FMT_COUNTERVALUE v = {};
    if (PdhGetFormattedCounterValue(hPagesIn_, PDH_FMT_DOUBLE, NULL, &v) == ERROR_SUCCESS)
        s.pagesInPerSec = v.doubleValue;
    if (PdhGetFormattedCounterValue(hPagesOut_, PDH_FMT_DOUBLE, NULL, &v) == ERROR_SUCCESS)
        s.pagesOutPerSec = v.doubleValue;
    return s;
}

KernelTableSample SystemSampler::sampleKernelTable() {
    KernelTableSample s;

    if (pdhQuery_.isValid()) {
        PdhCollectQueryData(pdhQuery_);
        PDH_FMT_COUNTERVALUE v = {};
        if (PdhGetFormattedCounterValue(hProcCount_, PDH_FMT_LONG, NULL, &v) == ERROR_SUCCESS)
            s.processCount = static_cast<uint32_t>(v.longValue);
        if (PdhGetFormattedCounterValue(hThreadCount_, PDH_FMT_LONG, NULL, &v) == ERROR_SUCCESS)
            s.threadCount = static_cast<uint32_t>(v.longValue);
    }

    DWORD procs[1024];
    DWORD bytesNeeded = 0;
    if (EnumProcesses(procs, sizeof(procs), &bytesNeeded)) {
        DWORD count = bytesNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < count; ++i) {
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, procs[i]);
            if (h) {
                DWORD handleCount = 0;
                if (GetProcessHandleCount(h, &handleCount)) {
                    s.handleCount += handleCount;
                }
                CloseHandle(h);
            }
        }
    }

    return s;
}

std::vector<CpuCoreSample> SystemSampler::sampleCpuCores() {
    std::vector<CpuCoreSample> samples;
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    DWORD count = info.dwNumberOfProcessors;

    for (DWORD core = 0; core < count; ++core) {
        HQUERY query = nullptr;
        HCOUNTER usr = nullptr;
        HCOUNTER sys = nullptr;
        HCOUNTER idle = nullptr;
        std::wstring coreLabel = std::to_wstring(static_cast<unsigned long long>(core));

        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) continue;

        std::wstring userPath = L"\\Processor(" + coreLabel + L")\\% User Time";
        std::wstring sysPath = L"\\Processor(" + coreLabel + L")\\% Privileged Time";
        std::wstring idlePath = L"\\Processor(" + coreLabel + L")\\% Idle Time";

        bool ok = PdhAddEnglishCounterW(query, userPath.c_str(), 0, &usr) == ERROR_SUCCESS &&
                  PdhAddEnglishCounterW(query, sysPath.c_str(), 0, &sys) == ERROR_SUCCESS &&
                  PdhAddEnglishCounterW(query, idlePath.c_str(), 0, &idle) == ERROR_SUCCESS;
        if (ok) {
            if (PdhCollectQueryData(query) == ERROR_SUCCESS) {
                PDH_FMT_COUNTERVALUE v = {};
                CpuCoreSample s{};
                s.core = core;
                if (PdhGetFormattedCounterValue(usr, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS) s.usr = v.doubleValue;
                if (PdhGetFormattedCounterValue(sys, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS) s.sys = v.doubleValue;
                if (PdhGetFormattedCounterValue(idle, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS) s.idle = v.doubleValue;
                samples.push_back(s);
            }
        }
        PdhCloseQuery(query);
    }
    return samples;
}
