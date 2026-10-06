#ifndef SAR_HPP
#define SAR_HPP

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

#endif // SAR_HPP
