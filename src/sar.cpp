/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Single-file index
 * ---------------------------------------------------------------------------
 * 1. Platform includes and linker directives
 * 2. Time / utility helpers
 * 3. PDH and Windows API wrapper types
 * 4. SarOptions and telemetry data structures
 * 5. SystemSampler implementation (CPU, memory, disk, paging, network, queue)
 * 6. Formatting helpers and report section printers
 * 7. CLI help and argument parsing
 * 8. Sampling loop and all-report rendering
 * ---------------------------------------------------------------------------
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include <sstream>
#include <ctime>
#include <numeric>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

// Helper: Convert FileTime to 64-bit integer
inline uint64_t FileTimeToQuad(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// Get current timestamp string (HH:MM:SS)
std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now_time);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S");
    return oss.str();
}

// Get system date string (MM/DD/YY)
std::string GetSystemDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now_time);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%m/%d/%y");
    return oss.str();
}

// Get Computer Hostname
std::string GetHostNameString() {
    char name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(name);
    if (GetComputerNameA(name, &size)) return std::string(name);
    return "WINDOWS-NT";
}

std::string WideToUtf8(const std::wstring& value);

// RAII Handle Wrapper for PDH Queries
class PdhQuery {
    HQUERY hQuery_ = nullptr;
public:
    PdhQuery() { PdhOpenQueryW(NULL, 0, &hQuery_); }
    ~PdhQuery() { if (hQuery_) PdhCloseQuery(hQuery_); }
    operator HQUERY() const { return hQuery_; }
    bool isValid() const { return hQuery_ != nullptr; }
};

// Flags for metric collection modes
struct SarOptions {
    bool cpu = false;        // -u
    bool memory = false;     // -r
    bool disk = false;       // -d
    bool diskDetail = false; // -D
    bool page = false;       // -w
    bool kernel = false;     // -v
    bool perCore = false;    // -M
    bool io = false;         // -b
    bool network = false;    // -n
    bool queue = false;      // -q
    bool swap = false;       // alias for page reporting
    bool all = false;        // -A
    bool csv = false;        // --csv
    bool json = false;       // --json
    int interval = 1;        // Seconds
    int count = 1;           // Samples count (0 = infinite)
};

// CPU Telemetry State
struct CpuSample {
    double usr = 0.0;
    double sys = 0.0;
    double wio = 0.0; // Interrupt / I/O wait equivalent
    double idle = 0.0;
};

// Memory Telemetry State
struct MemorySample {
    uint64_t freeMemKb = 0;
    uint64_t freeSwapKb = 0;
    double memUsedPct = 0.0;
    double swapUsedPct = 0.0;
    uint64_t totalMemKb = 0;
};

struct PagingSample {
    double pagesInPerSec = 0.0;
    double pagesOutPerSec = 0.0;
};

struct KernelTableSample {
    uint32_t processCount = 0;
    uint32_t threadCount = 0;
    uint32_t handleCount = 0;
};

struct CpuCoreSample {
    uint32_t core = 0;
    double usr = 0.0;
    double sys = 0.0;
    double idle = 0.0;
};

// Disk Telemetry State
struct DiskSample {
    std::string device = "_Total";
    double busyPct = 0.0;
    double avgQueue = 0.0;
    double transfersPerSec = 0.0;
    double blocksPerSec = 0.0;
    double avgWaitMs = 0.0;
    double avgServMs = 0.0;
};

// Network Telemetry State
struct NetSample {
    std::string iface = "ALL";
    double rxPckSec = 0.0;
    double txPckSec = 0.0;
    double rxKbSec = 0.0;
    double txKbSec = 0.0;
};

// Queue Telemetry State
struct QueueSample {
    double runqSz = 0.0;
    double runOccPct = 0.0;
    uint32_t processCount = 0;
    uint32_t threadCount = 0;
};

// System Sampler Engine
class SystemSampler {
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

public:
    SystemSampler() {
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
        InitNetworkStats();
    }

    void InitNetworkStats() {
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

    CpuSample SampleCpu() {
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
            s.wio = 0.0; // Windows does not block CPU on I/O wait in kernel state
        }
        return s;
    }

    MemorySample SampleMemory() {
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

    DiskSample SampleDisk() {
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
            s.blocksPerSec = val.doubleValue / 512.0; // Blocks (512 bytes) per second

        if (PdhGetFormattedCounterValue(hDiskLatency_, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS) {
            s.avgWaitMs = val.doubleValue * 1000.0;
            s.avgServMs = val.doubleValue * 1000.0;
        }
        return s;
    }

    std::vector<DiskSample> SampleDiskDetail() {
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

    NetSample SampleNetwork(double intervalSec) {
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

    QueueSample SampleQueue() {
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

    PagingSample SamplePaging() {
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

    KernelTableSample SampleKernelTable() {
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

    std::vector<CpuCoreSample> SampleCpuCores() {
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
};

void PrintHeaderLine() {
    std::cout << "sar (" << GetHostNameString()
              << ")   " << GetSystemDate() << "\n\n";
}

std::string FormatPercent(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << "%";
    return oss.str();
}

std::string FormatKib(uint64_t kib) {
    const double kibAsDouble = static_cast<double>(kib);

    std::ostringstream oss;
    if (kib >= 1024ULL * 1024ULL) {
        oss << std::fixed << std::setprecision(1) << (kibAsDouble / (1024.0 * 1024.0)) << "G";
    } else if (kib >= 1024ULL) {
        oss << std::fixed << std::setprecision(1) << (kibAsDouble / 1024.0) << "M";
    } else {
        oss << kib << "K";
    }
    return oss.str();
}

std::string FormatRate(double value, const std::string& unit) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << unit;
    return oss.str();
}

std::string FormatWaitMs(double ms) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << ms << "ms";
    return oss.str();
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

void PrintSectionHeader(const std::string& title, const std::vector<std::string>& columns) {
    std::cout << title << "\n";
    std::cout << std::left;
    std::cout << std::setw(8) << "time";
    for (const auto& column : columns) {
        std::cout << " " << std::setw(12) << column;
    }
    std::cout << "\n";
}

void PrintCsvSectionHeader(const std::string& title, const std::vector<std::string>& columns) {
    std::cout << "# " << title << "\n";
    std::cout << "time";
    for (const auto& column : columns) {
        std::cout << "," << column;
    }
    std::cout << "\n";
}

void PrintJsonSectionHeader(const std::string& title) {
    std::cout << "[\n";
    std::cout << "  {\"section\":\"" << title << "\"}\n";
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += ch; break;
        }
    }
    return out;
}

void PrintHelp() {
    std::cout <<
R"(sar(1)                   CrossShell for UNIX Reference Manual                    sar(1)

NAME
    sar - system activity reporter

SYNOPSIS
    sar [OPTIONS] [INTERVAL [COUNT]]

DESCRIPTION
    Reports comprehensive system activity statistics including CPU
    utilization, memory and swap usage, disk I/O, network interface
    statistics, paging activity, and process run-queue information.

OPTIONS
    -u
        Report CPU utilization (%usr, %sys, %wio, %idle). [Default]
    -r
        Report memory and swap utilization (freemem, freeswp, %memused).
    -d
        Report physical disk aggregate activity (%busy, avque, r+w/s, blks/s, avwait).
    -D
        Report per-device physical disk activity, one row per disk instance.
    -v
        Report kernel table activity (processes, threads, handles).
    -M
        Report per-core CPU utilization (%usr, %sys, %idle).
    -b
        Report block-device transfer statistics (HP-UX sar -b style).
    -n
        Report network interface activity (rxpck/s, txpck/s, rxkB/s, txkB/s).
    -q
        Report process run-queue length and active task stats.
    -w
        Report paging activity (pages in/out per second).
    --csv
        Emit CSV rows for script-friendly output.
    --json
        Emit JSON rows for script-friendly output.
    -A
        Report all supported sar-compatible system activity statistics.
    -h, --help, /?
        Display this comprehensive reference manual and exit.

EXAMPLES
    sar 1 5
        Sample CPU utilization every 1 second, 5 times.

    sar -r 2 10
        Sample memory statistics every 2 seconds, 10 times.

    sar -d 1 0
        Continuously sample disk I/O every 1 second.

    sar -M --csv 1 3
        Report per-core CPU in CSV format.

    sar -A 1 3
        Report complete sar-compatible diagnostics 3 times.

EXIT STATUS
    0   Success.
    1   Invalid options or sampling failure.

    CrossShell for UNIX                                                   sar(1)
)";
}

int main(int argc, char* argv[]) {
    SarOptions opts;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "-help" || arg == "/?" || arg == "--help") {
            PrintHelp();
            return 0;
        } else if (arg == "-u") opts.cpu = true;
        else if (arg == "-r") opts.memory = true;
        else if (arg == "-d") opts.disk = true;
        else if (arg == "-D") opts.diskDetail = true;
        else if (arg == "-v") opts.kernel = true;
        else if (arg == "-M") opts.perCore = true;
        else if (arg == "-b") opts.io = true;
        else if (arg == "-n") opts.network = true;
        else if (arg == "-q") opts.queue = true;
        else if (arg == "-w") opts.page = true;
        else if (arg == "--csv") opts.csv = true;
        else if (arg == "--json") opts.json = true;
        else if (arg == "-A") opts.all = true;
        else if (arg[0] != '-') positional.push_back(arg);
    }

    if (opts.swap) opts.page = true;
    if (opts.page) opts.swap = true;
    if (opts.io) opts.disk = true;
    if (opts.diskDetail) opts.disk = true;

    if (opts.all) {
        opts.cpu = opts.memory = opts.disk = opts.diskDetail = opts.page = opts.kernel = opts.perCore = opts.io = opts.network = opts.queue = opts.swap = true;
    } else if (!opts.cpu && !opts.memory && !opts.disk && !opts.diskDetail && !opts.page && !opts.kernel && !opts.perCore && !opts.io && !opts.network && !opts.queue && !opts.swap) {
        opts.cpu = true; // Default behavior when no flag specified
    }

    if (!positional.empty()) {
        opts.interval = std::stoi(positional[0]);
        if (positional.size() > 1) opts.count = std::stoi(positional[1]);
        else opts.count = 0; // Infinite loop if count omitted
    }

    PrintHeaderLine();
    if (!opts.all) {
        if (opts.csv || opts.json) {
            if (opts.cpu) PrintCsvSectionHeader("CPU", { "USR%", "SYS%", "WIO%", "IDLE%" });
            if (opts.memory) PrintCsvSectionHeader("MEMORY", { "FREEKB", "SWAPKB", "USED%", "SWP%", "TOTKB" });
            if (opts.disk || opts.io) PrintCsvSectionHeader("DISK", { "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            if (opts.diskDetail) PrintCsvSectionHeader("DISK DETAIL", { "DEV", "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            if (opts.page) PrintCsvSectionHeader("PAGE", { "PI/S", "PO/S" });
            if (opts.kernel) PrintCsvSectionHeader("KERNEL", { "PROC", "THREADS", "HANDLES" });
            if (opts.perCore) PrintCsvSectionHeader("CPU CORE", { "CORE", "USR%", "SYS%", "IDLE%" });
            if (opts.network) PrintCsvSectionHeader("NETWORK", { "RX/S", "TX/S", "RK/S", "TK/S" });
            if (opts.queue) PrintCsvSectionHeader("QUEUE", { "RUNQ", "OCC%", "PROC", "THR" });
        } else {
            if (opts.cpu) PrintSectionHeader("CPU", { "%USR", "%SYS", "%WIO", "%IDLE" });
            if (opts.memory) PrintSectionHeader("MEMORY", { "FREEKB", "SWAPKB", "%USED", "%SWP", "TOTKB" });
            if (opts.disk || opts.io) PrintSectionHeader("DISK", { "%BUSY", "AVGQ", "R+W/S", "BLKS/S", "AVWAIT" });
            if (opts.diskDetail) PrintSectionHeader("DISK DETAIL", { "DEV", "%BUSY", "AVGQ", "X/S", "B/S", "WAIT" });
            if (opts.page) PrintSectionHeader("PAGE", { "PI/S", "PO/S" });
            if (opts.kernel) PrintSectionHeader("KERNEL", { "PROC", "THREADS", "HANDLES" });
            if (opts.perCore) PrintSectionHeader("CPU CORE", { "CORE", "USR%", "SYS%", "IDLE%" });
            if (opts.network) PrintSectionHeader("NETWORK", { "RXPCK/S", "TXPCK/S", "RXKB/S", "TXKB/S" });
            if (opts.queue) PrintSectionHeader("QUEUE", { "RUNQ", "%RUNOCC", "PROCS", "THREADS" });
        }
    }

    SystemSampler sampler;

    // Warm-up delay for delta counters
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Summary accumulator arrays
    std::vector<CpuSample> cpuHistory;
    std::vector<MemorySample> memHistory;
    std::vector<DiskSample> diskHistory;
    std::vector<NetSample> netHistory;
    std::vector<QueueSample> queueHistory;
    std::vector<PagingSample> pageHistory;
    std::vector<KernelTableSample> kernelHistory;
    std::vector<CpuCoreSample> perCoreHistory;
    std::vector<std::string> cpuTimes;
    std::vector<std::string> memTimes;
    std::vector<std::string> diskTimes;
    std::vector<std::string> netTimes;
    std::vector<std::string> queueTimes;
    std::vector<std::string> pageTimes;
    std::vector<std::string> kernelTimes;
    std::vector<std::string> perCoreTimes;

    int currentSample = 0;
    while (opts.count == 0 || currentSample < opts.count) {
        std::string ts = GetCurrentTimestamp();

        // 1. CPU Report (-u)
        if (opts.cpu) {
            auto s = sampler.SampleCpu();
            cpuHistory.push_back(s);
            cpuTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.usr << "," << s.sys << "," << s.wio << "," << s.idle << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"CPU\",\"usr%\":" << s.usr << ",\"sys%\":" << s.sys << ",\"wio%\":" << s.wio << ",\"idle%\":" << s.idle << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " usr=" << std::setw(7) << FormatPercent(s.usr)
                              << " sys=" << std::setw(7) << FormatPercent(s.sys)
                              << " wio=" << std::setw(7) << FormatPercent(s.wio)
                              << " idle=" << std::setw(7) << FormatPercent(s.idle) << "\n";
                }
            }
        }

        // 2. Memory Report (-r)
        if (opts.memory) {
            auto s = sampler.SampleMemory();
            memHistory.push_back(s);
            memTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.freeMemKb << "," << s.freeSwapKb << "," << s.memUsedPct << "," << s.swapUsedPct << "," << s.totalMemKb << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"MEMORY\",\"freeKB\":" << s.freeMemKb << ",\"swapKB\":" << s.freeSwapKb << ",\"used%\":" << s.memUsedPct << ",\"swp%\":" << s.swapUsedPct << ",\"totalKB\":" << s.totalMemKb << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " free=" << std::setw(7) << FormatKib(s.freeMemKb)
                              << " swap=" << std::setw(7) << FormatKib(s.freeSwapKb)
                              << " used=" << std::setw(7) << FormatPercent(s.memUsedPct)
                              << " swp=" << std::setw(7) << FormatPercent(s.swapUsedPct)
                              << " total=" << std::setw(7) << FormatKib(s.totalMemKb) << "\n";
                }
            }
        }

        // 3. Disk / Block I/O Report (-d, -D, -b)
        if (opts.disk || opts.io) {
            auto s = sampler.SampleDisk();
            diskHistory.push_back(s);
            diskTimes.push_back(ts);
            if (!opts.all) {
                std::cout << std::left << std::setw(8) << ts
                          << " busy=" << std::setw(7) << FormatPercent(s.busyPct)
                          << " avgq=" << std::setw(6) << std::fixed << std::setprecision(1) << s.avgQueue
                          << " xfers=" << std::setw(7) << FormatRate(s.transfersPerSec, "/s")
                          << " blks=" << std::setw(7) << FormatRate(s.blocksPerSec, "/s")
                          << " wait=" << std::setw(7) << std::fixed << std::setprecision(1) << s.avgWaitMs << "ms\n";
            }
        }

        if (opts.diskDetail) {
            auto detail = sampler.SampleDiskDetail();
            if (!opts.all) {
                for (const auto& d : detail) {
                    std::cout << std::left << std::setw(8) << ts
                              << " dev=" << std::setw(6) << d.device
                              << " busy=" << std::setw(7) << FormatPercent(d.busyPct)
                              << " avgq=" << std::setw(6) << std::fixed << std::setprecision(1) << d.avgQueue
                              << " xfers=" << std::setw(7) << FormatRate(d.transfersPerSec, "/s")
                              << " blks=" << std::setw(7) << FormatRate(d.blocksPerSec, "/s")
                              << " wait=" << std::setw(7) << std::fixed << std::setprecision(1) << d.avgWaitMs << "ms\n";
                }
            }
        }

        if (opts.page) {
            auto s = sampler.SamplePaging();
            pageHistory.push_back(s);
            pageTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.pagesInPerSec << "," << s.pagesOutPerSec << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"PAGE\",\"pi/s\":" << s.pagesInPerSec << ",\"po/s\":" << s.pagesOutPerSec << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " pi/s=" << std::setw(7) << FormatRate(s.pagesInPerSec, "/s")
                              << " po/s=" << std::setw(7) << FormatRate(s.pagesOutPerSec, "/s") << "\n";
                }
            }
        }

        if (opts.kernel) {
            auto s = sampler.SampleKernelTable();
            kernelHistory.push_back(s);
            kernelTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.processCount << "," << s.threadCount << "," << s.handleCount << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"KERNEL\",\"proc\":" << s.processCount << ",\"threads\":" << s.threadCount << ",\"handles\":" << s.handleCount << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " proc=" << std::setw(5) << s.processCount
                              << " threads=" << std::setw(5) << s.threadCount
                              << " handles=" << std::setw(6) << s.handleCount << "\n";
                }
            }
        }

        if (opts.perCore) {
            auto samples = sampler.SampleCpuCores();
            for (const auto& s : samples) {
                perCoreHistory.push_back(s);
                perCoreTimes.push_back(ts);
                if (!opts.all) {
                    if (opts.csv) {
                        std::cout << ts << "," << s.core << "," << s.usr << "," << s.sys << "," << s.idle << "\n";
                    } else if (opts.json) {
                        std::cout << "{\"TIME\":\"" << ts << "\",\"SECTION\":\"CPU CORE\",\"CORE\":" << s.core << ",\"USR%\":" << s.usr << ",\"SYS%\":" << s.sys << ",\"IDLE%\":" << s.idle << "}\n";
                    } else {
                        std::cout << std::left << std::setw(8) << ts
                                  << " core=" << std::setw(3) << s.core
                                  << " usr=" << std::setw(7) << FormatPercent(s.usr)
                                  << " sys=" << std::setw(7) << FormatPercent(s.sys)
                                  << " idle=" << std::setw(7) << FormatPercent(s.idle) << "\n";
                    }
                }
            }
        }

        // 4. Network Report (-n)
        if (opts.network) {
            auto s = sampler.SampleNetwork(opts.interval);
            netHistory.push_back(s);
            netTimes.push_back(ts);
            if (!opts.all) {
                std::cout << std::left << std::setw(8) << ts
                          << " rx=" << std::setw(7) << FormatRate(s.rxPckSec, "/s")
                          << " tx=" << std::setw(7) << FormatRate(s.txPckSec, "/s")
                          << " rKB=" << std::setw(7) << FormatRate(s.rxKbSec, "K/s")
                          << " tKB=" << std::setw(7) << FormatRate(s.txKbSec, "K/s") << "\n";
            }
        }

        // 5. Queue Report (-q)
        if (opts.queue) {
            auto s = sampler.SampleQueue();
            queueHistory.push_back(s);
            queueTimes.push_back(ts);
            if (!opts.all) {
                std::cout << std::left << std::setw(8) << ts
                          << " runq=" << std::setw(5) << std::fixed << std::setprecision(1) << s.runqSz
                          << " occ=" << std::setw(6) << FormatPercent(s.runOccPct)
                          << " procs=" << std::setw(5) << s.processCount
                          << " threads=" << std::setw(5) << s.threadCount << "\n";
            }
        }

        currentSample++;
        if (opts.count == 0 || currentSample < opts.count) {
            std::this_thread::sleep_for(std::chrono::seconds(opts.interval));
        }
    }

    if (opts.all) {
        if (opts.cpu) {
            PrintSectionHeader("CPU", { "USR%", "SYS%", "WIO%", "IDLE%" });
            for (size_t i = 0; i < cpuHistory.size(); ++i) {
                const auto& s = cpuHistory[i];
                std::cout << std::left << std::setw(8) << cpuTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatPercent(s.usr)
                          << " " << std::setw(12) << FormatPercent(s.sys)
                          << " " << std::setw(12) << FormatPercent(s.wio)
                          << " " << std::setw(12) << FormatPercent(s.idle) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.memory) {
            PrintSectionHeader("MEMORY", { "FREE", "SWAP", "USED%", "SWP%", "TOTAL" });
            for (size_t i = 0; i < memHistory.size(); ++i) {
                const auto& s = memHistory[i];
                std::cout << std::left << std::setw(8) << memTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatKib(s.freeMemKb)
                          << " " << std::setw(12) << FormatKib(s.freeSwapKb)
                          << " " << std::setw(12) << FormatPercent(s.memUsedPct)
                          << " " << std::setw(12) << FormatPercent(s.swapUsedPct)
                          << " " << std::setw(12) << FormatKib(s.totalMemKb) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.disk || opts.io) {
            PrintSectionHeader("DISK", { "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            for (size_t i = 0; i < diskHistory.size(); ++i) {
                const auto& s = diskHistory[i];
                std::cout << std::left << std::setw(8) << diskTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatPercent(s.busyPct)
                          << " " << std::setw(12) << s.avgQueue
                          << " " << std::setw(12) << FormatRate(s.transfersPerSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.blocksPerSec, "/s")
                          << " " << std::setw(12) << FormatWaitMs(s.avgWaitMs) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.diskDetail) {
            PrintSectionHeader("DISK DETAIL", { "DEVICE", "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            std::vector<DiskSample> detail;
            for (size_t i = 0; i < diskTimes.size() && i < 1; ++i) {
                detail = sampler.SampleDiskDetail();
                for (const auto& d : detail) {
                    std::cout << std::left << std::setw(8) << diskTimes[i]
                              << " " << std::left << std::fixed << std::setprecision(1)
                              << std::setw(12) << d.device
                              << " " << std::setw(12) << FormatPercent(d.busyPct)
                              << " " << std::setw(12) << d.avgQueue
                              << " " << std::setw(12) << FormatRate(d.transfersPerSec, "/s")
                              << " " << std::setw(12) << FormatRate(d.blocksPerSec, "/s")
                              << " " << std::setw(12) << FormatWaitMs(d.avgWaitMs) << "\n";
                }
            }
            std::cout << "\n";
        }
        if (opts.page) {
            PrintSectionHeader("PAGE", { "PI/S", "PO/S" });
            for (size_t i = 0; i < pageHistory.size(); ++i) {
                const auto& s = pageHistory[i];
                std::cout << std::left << std::setw(8) << pageTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatRate(s.pagesInPerSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.pagesOutPerSec, "/s") << "\n";
            }
            std::cout << "\n";
        }
        if (opts.kernel) {
            PrintSectionHeader("KERNEL", { "PROC", "THREADS", "HANDLES" });
            for (size_t i = 0; i < kernelHistory.size(); ++i) {
                const auto& s = kernelHistory[i];
                std::cout << std::left << std::setw(8) << kernelTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << s.processCount
                          << " " << std::setw(12) << s.threadCount
                          << " " << std::setw(12) << s.handleCount << "\n";
            }
            std::cout << "\n";
        }
        if (opts.perCore) {
            PrintSectionHeader("CPU CORE", { "CORE", "USR%", "SYS%", "IDLE%" });
            for (size_t i = 0; i < perCoreHistory.size(); ++i) {
                const auto& s = perCoreHistory[i];
                std::cout << std::left << std::setw(8) << perCoreTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << s.core
                          << " " << std::setw(12) << FormatPercent(s.usr)
                          << " " << std::setw(12) << FormatPercent(s.sys)
                          << " " << std::setw(12) << FormatPercent(s.idle) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.network) {
            PrintSectionHeader("NETWORK", { "RX/S", "TX/S", "RK/S", "TK/S" });
            for (size_t i = 0; i < netHistory.size(); ++i) {
                const auto& s = netHistory[i];
                std::cout << std::left << std::setw(8) << netTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatRate(s.rxPckSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.txPckSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.rxKbSec, "K/s")
                          << " " << std::setw(12) << FormatRate(s.txKbSec, "K/s") << "\n";
            }
            std::cout << "\n";
        }
        if (opts.queue) {
            PrintSectionHeader("QUEUE", { "RUNQ", "OCC%", "PROC", "THR" });
            for (size_t i = 0; i < queueHistory.size(); ++i) {
                const auto& s = queueHistory[i];
                std::cout << std::left << std::setw(8) << queueTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << s.runqSz
                          << " " << std::setw(12) << FormatPercent(s.runOccPct)
                          << " " << std::setw(12) << s.processCount
                          << " " << std::setw(12) << s.threadCount << "\n";
            }
            std::cout << "\n";
        }
    }

    // Print Averages Line (HP-UX Standard Behavior)
    if (currentSample > 1) {
        std::cout << "\n";
        if (opts.cpu) {
            double u = 0, sys = 0, w = 0, id = 0;
            for (const auto& c : cpuHistory) { u += c.usr; sys += c.sys; w += c.wio; id += c.idle; }
            size_t n = cpuHistory.size();
            std::cout << std::left << std::setw(10) << "Average"
                      << std::right << std::fixed << std::setprecision(1)
                      << std::setw(8) << (u / n) << std::setw(8) << (sys / n)
                      << std::setw(8) << (w / n) << std::setw(8) << (id / n) << "  (CPU %USR %SYS %WIO %IDLE)\n";
        }
    }

    return 0;
}