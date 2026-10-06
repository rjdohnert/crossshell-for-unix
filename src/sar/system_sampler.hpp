#ifndef SYSTEM_SAMPLER_HPP
#define SYSTEM_SAMPLER_HPP

#include "sar.hpp"

// RAII Handle Wrapper for PDH Queries
class PdhQuery {
    HQUERY hQuery_ = nullptr;
public:
    PdhQuery() { PdhOpenQueryW(NULL, 0, &hQuery_); }
    ~PdhQuery() { if (hQuery_) PdhCloseQuery(hQuery_); }
    operator HQUERY() const { return hQuery_; }
    bool isValid() const { return hQuery_ != nullptr; }
};

class SystemSampler {
private:
    FILETIME prevIdle_{}, prevKernel_{}, prevUser_{};
    
    // PDH Counter Handles for Disk & Queue
    PdhQuery pdhQuery_;
    HCOUNTER hDiskBusy_ = nullptr;
    HCOUNTER hDiskQueue_ = nullptr;
    HCOUNTER hDiskXfers_ = nullptr;
    HCOUNTER hDiskBytes_ = nullptr;
    HCOUNTER hDiskLatency_ = nullptr;

    HCOUNTER hPagesIn_ = nullptr;
    HCOUNTER hPagesOut_ = nullptr;
    HCOUNTER hProcQueue_ = nullptr;
    HCOUNTER hProcCount_ = nullptr;
    HCOUNTER hThreadCount_ = nullptr;

    // Network Prev Counters
    uint64_t prevRxBytes_ = 0, prevTxBytes_ = 0;
    uint64_t prevRxPackets_ = 0, prevTxPackets_ = 0;

    double prevPagesIn_ = 0.0;
    double prevPagesOut_ = 0.0;

    void initNetworkStats();

public:
    SystemSampler();

    CpuSample sampleCpu();
    MemorySample sampleMemory();
    DiskSample sampleDisk();
    std::vector<DiskSample> sampleDiskDetail();
    NetSample sampleNetwork(double intervalSec);
    QueueSample sampleQueue();
    PagingSample samplePaging();
    KernelTableSample sampleKernelTable();
    std::vector<CpuCoreSample> sampleCpuCores();
};

#endif // SYSTEM_SAMPLER_HPP
