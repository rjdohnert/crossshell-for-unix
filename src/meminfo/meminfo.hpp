#ifndef MEMINFO_HPP
#define MEMINFO_HPP

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdint>

struct PageDeviceInfo {
    std::wstring path;
    uint64_t totalBytes{0};
    uint64_t usedBytes{0};
    uint64_t peakBytes{0};
};

struct MemorySnapshot {
    uint64_t totalPhysical{0};
    uint64_t availPhysical{0};
    uint64_t usedPhysical{0};
    uint64_t systemCache{0};

    uint64_t commitTotal{0};
    uint64_t commitLimit{0};
    uint64_t commitPeak{0};

    uint64_t kernelPaged{0};
    uint64_t kernelNonPaged{0};

    std::vector<PageDeviceInfo> pageFiles;
};

enum class DisplayUnit { Human, Bytes, Kibi, Mebi, Gibi };
enum class OutputFormat { Table, Json, Csv };

struct Config {
    DisplayUnit unit = DisplayUnit::Human;
    OutputFormat format = OutputFormat::Table;
    bool showPagefileDetails = false;
    bool showKernelDetails = false;
    int repeatSeconds = 0;
    int maxCount = 1;
    bool showHelp = false;
    bool showVersion = false;
};

#endif // MEMINFO_HPP
