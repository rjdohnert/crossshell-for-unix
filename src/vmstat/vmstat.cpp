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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * vmstat - Virtual Memory Statistics
 *
 * Production-ready Windows CLI utility providing real-time system performance,
 * virtual memory paging, process queue, and CPU utilization metrics.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <csignal>
#include <cmath>
#include <cerrno>
#include <climits>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Pdh.lib")

// Metadata
constexpr const char* PROGRAM_NAME = "vmstat";
constexpr const char* VERSION      = "1.0.0";
constexpr const char* COPYRIGHT    = "Copyright (C) 2026";

/// ============================================================================
// 1. SYSTEM METRICS & NT/PDH TYPEDEFS
// ============================================================================

typedef LONG NTSTATUS;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

typedef struct _SYSTEM_PERFORMANCE_INFORMATION_FULL {
    LARGE_INTEGER IdleProcessTime;
    LARGE_INTEGER IoReadTransferCount;
    LARGE_INTEGER IoWriteTransferCount;
    LARGE_INTEGER IoOtherTransferCount;
    ULONG IoReadOperationCount;
    ULONG IoWriteOperationCount;
    ULONG IoOtherOperationCount;
    ULONG AvailablePages;
    ULONG CommittedPages;
    ULONG CommitLimit;
    ULONG PeakCommitment;
    ULONG PageFaultCount;
    ULONG CopyOnWriteCount;
    ULONG TransitionCount;
    ULONG CacheTransitionCount;
    ULONG DemandZeroCount;
    ULONG PageReadCount;
    ULONG PageReadIoCount;
    ULONG DirectoryAvailableBufferPages;
    ULONG SystemCachePageCount;
    ULONG FreeBufferPages;
    ULONG PagefilePagesWritten;
    ULONG PagefilePageWriteIoCount;
    ULONG MappedFilePagesWritten;
    ULONG MappedFilePageWriteIoCount;
    ULONG PagedPoolPages;
    ULONG NonPagedPoolPages;
    ULONG PagedPoolAllocs;
    ULONG PagedPoolFrees;
    ULONG NonPagedPoolAllocs;
    ULONG NonPagedPoolFrees;
    ULONG FreeSystemPtes;
    ULONG ResidentSystemCodePage;
    ULONG TotalSystemDriverPages;
    ULONG TotalSystemCodePages;
    ULONG NonPagedPoolLookasideHits;
    ULONG PagedPoolLookasideHits;
    ULONG AvailablePagedPoolPages;
    ULONG ResidentSystemCachePage;
    ULONG ResidentPagedPoolPage;
    ULONG ResidentSystemDriverPage;
    ULONG CcFastReadNoWait;
    ULONG CcFastReadWait;
    ULONG CcFastReadResourceMiss;
    ULONG CcFastReadNotPossible;
    ULONG CcFastMdlReadNoWait;
    ULONG CcFastMdlReadWait;
    ULONG CcFastMdlReadResourceMiss;
    ULONG CcFastMdlReadNotPossible;
    ULONG CcMapDataNoWait;
    ULONG CcMapDataWait;
    ULONG CcMapDataWaitMiss;
    ULONG CcPinMappedDataCount;
    ULONG CcPinReadNoWait;
    ULONG CcPinReadWait;
    ULONG CcPinReadHeaderMiss;
    ULONG CcCopyReadNoWait;
    ULONG CcCopyReadWait;
    ULONG CcCopyReadMiss;
    ULONG CcMdlReadNoWait;
    ULONG CcMdlReadWait;
    ULONG CcMdlReadResourceMiss;
    ULONG CcMdlReadNotPossible;
    ULONG CcReadAheadIos;
    ULONG CcLazyWriteIos;
    ULONG CcLazyWritePages;
    ULONG CcDataFlushes;
    ULONG CcDataPages;
    ULONG ContextSwitches;
    ULONG FirstLevelTbFills;
    ULONG SecondLevelTbFills;
    ULONG SystemCalls;
} SYSTEM_PERFORMANCE_INFORMATION_FULL;

typedef NTSTATUS(NTAPI* pfnNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

enum class UnitMode { Kilobytes, Megabytes, Human };
enum class OutputMode { Table, Csv, Json };
enum class LayoutMode { Auto, Compact, SingleLine };

struct TableLayout {
    int runWidth = 4;
    int blkWidth = 4;
    int thrWidth = 8;
    int memWidth = 10;
    int rateWidth = 6;
    int inWidth = 6;
    int cpuWidth = 4;
    bool compactBanner = false;
    bool splitRows = false;
};

struct PdhRateSample {
    bool anyValid = false;
    bool fltValid = false;
    bool pagesInValid = false;
    bool pagesOutValid = false;
    bool interruptsValid = false;
    bool sysCallsValid = false;
    bool ctxSwitchValid = false;
    double fltPerSec = 0.0;
    double pagesInPerSec = 0.0;
    double pagesOutPerSec = 0.0;
    double interruptsPerSec = 0.0;
    double sysCallsPerSec = 0.0;
    double ctxSwitchPerSec = 0.0;
};

struct PdhFallbackState {
    HQUERY query = nullptr;
    HCOUNTER pageFaults = nullptr;
    HCOUNTER pagesIn = nullptr;
    HCOUNTER pagesOut = nullptr;
    HCOUNTER interrupts = nullptr;
    HCOUNTER systemCalls = nullptr;
    HCOUNTER contextSwitches = nullptr;
    bool initialized = false;
};

struct SystemSample {
    std::chrono::steady_clock::time_point timestamp;
    bool ntPerfValid = false;
    SYSTEM_PERFORMANCE_INFORMATION_FULL perfInfo{};
    PERFORMANCE_INFORMATION psApiPerf{};
    ULARGE_INTEGER idleTime{};
    ULARGE_INTEGER kernelTime{};
    ULARGE_INTEGER userTime{};
};

struct VmstatOptions {
    UnitMode mode = UnitMode::Kilobytes;
    OutputMode outputMode = OutputMode::Table;
    LayoutMode layoutMode = LayoutMode::Auto;
    bool summaryMode = false;
    int interval = 0;
    int maxCount = -1;
    int headerInterval = 20;
    std::vector<std::string> positionalArgs;
};

// ============================================================================
// 2. STRING UTILITIES & FORMATTING
// ============================================================================

class StringUtils {
public:
    static std::string WStrToStr(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);
        if (size <= 0) return "";
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &str[0], size, NULL, NULL);
        return str;
    }

    static bool TryParseInt(const std::string& text, int minValue, int& outValue) {
        if (text.empty()) return false;
        errno = 0;
        char* end = nullptr;
        long parsed = std::strtol(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0' || errno == ERANGE || parsed < minValue || parsed > INT_MAX) {
            return false;
        }
        outValue = static_cast<int>(parsed);
        return true;
    }

    static std::string FormatLongOrNA(bool available, long value, int width) {
        std::ostringstream ss;
        if (available) {
            ss << std::setw(width) << value;
        } else {
            ss << std::setw(width) << "N/A";
        }
        return ss.str();
    }

    static std::string FormatValueRaw(uint64_t bytes, UnitMode mode) {
        char buf[64];
        if (mode == UnitMode::Human) {
            const char* units[] = { "B", "K", "M", "G", "T" };
            double val = static_cast<double>(bytes);
            int idx = 0;
            while (val >= 1024.0 && idx < 4) {
                val /= 1024.0;
                idx++;
            }
            if (idx == 0) sprintf_s(buf, "%lluB", bytes);
            else sprintf_s(buf, "%.1f%s", val, units[idx]);
        } else if (mode == UnitMode::Megabytes) {
            sprintf_s(buf, "%llu", bytes / (1024 * 1024));
        } else {
            sprintf_s(buf, "%llu", bytes / 1024);
        }
        return buf;
    }

    static std::string FormatValue(uint64_t bytes, UnitMode mode, int width) {
        std::string raw = FormatValueRaw(bytes, mode);
        std::ostringstream ss;
        ss << std::setw(width) << raw;
        return ss.str();
    }
};

// ============================================================================
// 3. CONSOLE & LAYOUT CONTROLLER
// ============================================================================

class ConsoleManager {
public:
    static int GetConsoleWidth() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return 120;
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return 120;
        return static_cast<int>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    }

    static TableLayout BuildTableLayout(int consoleWidth, LayoutMode layoutMode) {
        TableLayout layout{};
        if (layoutMode == LayoutMode::Compact) {
            layout.runWidth = 4;
            layout.blkWidth = 4;
            layout.thrWidth = 8;
            layout.memWidth = 10;
            layout.rateWidth = 6;
            layout.inWidth = 6;
            layout.cpuWidth = 4;
            layout.compactBanner = true;
            layout.splitRows = true;
            return layout;
        }

        if (consoleWidth < 105) {
            layout.runWidth = 3;
            layout.blkWidth = 3;
            layout.thrWidth = 6;
            layout.memWidth = 8;
            layout.rateWidth = 5;
            layout.inWidth = 4;
            layout.cpuWidth = 3;
            layout.compactBanner = true;
            layout.splitRows = true;
        } else if (consoleWidth < 130) {
            layout.runWidth = 4;
            layout.blkWidth = 4;
            layout.thrWidth = 8;
            layout.memWidth = 10;
            layout.rateWidth = 6;
            layout.inWidth = 6;
            layout.cpuWidth = 4;
            layout.compactBanner = true;
            layout.splitRows = false;
        } else {
            layout.runWidth = 6;
            layout.blkWidth = 6;
            layout.thrWidth = 10;
            layout.memWidth = 12;
            layout.rateWidth = 8;
            layout.inWidth = 8;
            layout.cpuWidth = 5;
            layout.compactBanner = (consoleWidth < 140);
            layout.splitRows = false;
        }

        if (layoutMode == LayoutMode::SingleLine) {
            layout.splitRows = false;
        }

        return layout;
    }
};

// ============================================================================
// 4. PERFORMANCE COUNTERS (NTDLL & PDH)
// ============================================================================

class PdhFallbackEngine {
private:
    PdhFallbackState m_pdh;

    static bool AddPdhCounter(HQUERY query, const char* path, HCOUNTER& counter) {
        PDH_STATUS status = PdhAddEnglishCounterA(query, path, 0, &counter);
        if (status == ERROR_SUCCESS) return true;
        status = PdhAddCounterA(query, path, 0, &counter);
        return status == ERROR_SUCCESS;
    }

    static bool ReadPdhCounterDouble(HCOUNTER counter, double& value) {
        PDH_FMT_COUNTERVALUE fmt{};
        PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &fmt);
        if (status != ERROR_SUCCESS) return false;
        if (fmt.CStatus != PDH_CSTATUS_VALID_DATA && fmt.CStatus != PDH_CSTATUS_NEW_DATA) return false;
        value = fmt.doubleValue;
        return true;
    }

public:
    bool Init() {
        if (m_pdh.initialized) return true;
        if (PdhOpenQueryW(nullptr, 0, &m_pdh.query) != ERROR_SUCCESS) return false;

        bool ok = true;
        ok = ok && AddPdhCounter(m_pdh.query, "\\Memory\\Page Faults/sec", m_pdh.pageFaults);
        ok = ok && AddPdhCounter(m_pdh.query, "\\Memory\\Pages Input/sec", m_pdh.pagesIn);
        ok = ok && AddPdhCounter(m_pdh.query, "\\Memory\\Pages Output/sec", m_pdh.pagesOut);
        ok = ok && AddPdhCounter(m_pdh.query, "\\Processor(_Total)\\Interrupts/sec", m_pdh.interrupts);
        ok = ok && AddPdhCounter(m_pdh.query, "\\System\\System Calls/sec", m_pdh.systemCalls);
        ok = ok && AddPdhCounter(m_pdh.query, "\\System\\Context Switches/sec", m_pdh.contextSwitches);

        if (!ok || PdhCollectQueryData(m_pdh.query) != ERROR_SUCCESS) {
            PdhCloseQuery(m_pdh.query);
            m_pdh = PdhFallbackState{};
            return false;
        }

        m_pdh.initialized = true;
        return true;
    }

    bool CollectRates(PdhRateSample& sample) {
        sample = PdhRateSample{};
        if (!m_pdh.initialized) return false;
        if (PdhCollectQueryData(m_pdh.query) != ERROR_SUCCESS) return false;

        sample.fltValid = ReadPdhCounterDouble(m_pdh.pageFaults, sample.fltPerSec);
        sample.pagesInValid = ReadPdhCounterDouble(m_pdh.pagesIn, sample.pagesInPerSec);
        sample.pagesOutValid = ReadPdhCounterDouble(m_pdh.pagesOut, sample.pagesOutPerSec);
        sample.interruptsValid = ReadPdhCounterDouble(m_pdh.interrupts, sample.interruptsPerSec);
        sample.sysCallsValid = ReadPdhCounterDouble(m_pdh.systemCalls, sample.sysCallsPerSec);
        sample.ctxSwitchValid = ReadPdhCounterDouble(m_pdh.contextSwitches, sample.ctxSwitchPerSec);

        sample.anyValid = sample.fltValid || sample.pagesInValid || sample.pagesOutValid
                       || sample.interruptsValid || sample.sysCallsValid || sample.ctxSwitchValid;
        return sample.anyValid;
    }

    void Shutdown() {
        if (m_pdh.query != nullptr) {
            PdhCloseQuery(m_pdh.query);
        }
        m_pdh = PdhFallbackState{};
    }

    bool IsInitialized() const {
        return m_pdh.initialized;
    }
};

class NtApiEngine {
private:
    pfnNtQuerySystemInformation m_ntQuery = nullptr;

public:
    bool Init() {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return false;
        m_ntQuery = reinterpret_cast<pfnNtQuerySystemInformation>(GetProcAddress(hNtdll, "NtQuerySystemInformation"));
        return m_ntQuery != nullptr;
    }

    bool TakeSample(SystemSample& sample) {
        sample.timestamp = std::chrono::steady_clock::now();
        sample.ntPerfValid = false;
        ZeroMemory(&sample.perfInfo, sizeof(sample.perfInfo));

        if (m_ntQuery) {
            NTSTATUS status = m_ntQuery(2, &sample.perfInfo, sizeof(sample.perfInfo), nullptr);
            sample.ntPerfValid = (status == STATUS_SUCCESS);
        }

        sample.psApiPerf.cb = sizeof(PERFORMANCE_INFORMATION);
        if (!GetPerformanceInfo(&sample.psApiPerf, sizeof(PERFORMANCE_INFORMATION))) {
            return false;
        }

        FILETIME ftIdle, ftKernel, ftUser;
        if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
            return false;
        }

        sample.idleTime.LowPart   = ftIdle.dwLowDateTime;
        sample.idleTime.HighPart  = ftIdle.dwHighDateTime;
        sample.kernelTime.LowPart = ftKernel.dwLowDateTime;
        sample.kernelTime.HighPart= ftKernel.dwHighDateTime;
        sample.userTime.LowPart   = ftUser.dwLowDateTime;
        sample.userTime.HighPart  = ftUser.dwHighDateTime;

        return true;
    }

    static double ClampRate(double value) {
        if (value < 0.0) return 0.0;
        if (value > 1000000000.0) return 1000000000.0;
        return value;
    }

    static double ComputeDeltaRate(ULONGLONG curr, ULONGLONG prev, double dt) {
        if (dt <= 0.0 || curr < prev) return 0.0;
        return ClampRate(static_cast<double>(curr - prev) / dt);
    }

    static const char* SelectSourceTag(bool ntAvailable, bool pdhAvailable) {
        if (ntAvailable) return "NT";
        if (pdhAvailable) return "PDH";
        return "N/A";
    }
};

// ============================================================================
// 5. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void PrintHeader(UnitMode mode, LayoutMode layoutMode) {
        std::string unitStr = (mode == UnitMode::Megabytes) ? "(MB)" : (mode == UnitMode::Human ? "(Auto)" : "(KB)");
        int consoleWidth = ConsoleManager::GetConsoleWidth();
        TableLayout layout = ConsoleManager::BuildTableLayout(consoleWidth, layoutMode);

        if (layout.compactBanner) {
            std::cout << "procs | memory " << unitStr << " | paging(/s) | faults(/s) | cpu(%)\n";
        } else {
            std::cout << "----procs---- "
                      << "------------------------memory " << std::left << std::setw(7) << unitStr << std::right << "------------------------ "
                      << "------paging (/s)------ "
                      << "------faults (/s)------ "
                      << "---cpu (%)---\n";
        }

        if (layout.splitRows) {
            std::cout << std::right
                      << std::setw(layout.runWidth) << "run" << ' '
                      << std::setw(layout.blkWidth) << "blk" << ' '
                      << std::setw(layout.thrWidth) << "thr" << ' '
                      << std::setw(layout.memWidth) << "swpd" << ' '
                      << std::setw(layout.memWidth) << "free" << ' '
                      << std::setw(layout.memWidth) << "buff" << ' '
                      << std::setw(layout.memWidth) << "cache" << "\n"
                      << std::setw(layout.rateWidth) << "flt" << ' '
                      << std::setw(layout.rateWidth) << "pi" << ' '
                      << std::setw(layout.rateWidth) << "po" << ' '
                      << std::setw(layout.inWidth) << "in" << ' '
                      << std::setw(layout.rateWidth) << "sy" << ' '
                      << std::setw(layout.rateWidth) << "cs" << ' '
                      << std::setw(layout.cpuWidth) << "us" << ' '
                      << std::setw(layout.cpuWidth) << "sy" << ' '
                      << std::setw(layout.cpuWidth) << "id"
                      << "\n";
        } else {
            std::cout << std::right
                      << std::setw(layout.runWidth) << "run" << ' '
                      << std::setw(layout.blkWidth) << "blk" << ' '
                      << std::setw(layout.thrWidth) << "thr" << ' '
                      << std::setw(layout.memWidth) << "swpd" << ' '
                      << std::setw(layout.memWidth) << "free" << ' '
                      << std::setw(layout.memWidth) << "buff" << ' '
                      << std::setw(layout.memWidth) << "cache" << ' '
                      << std::setw(layout.rateWidth) << "flt" << ' '
                      << std::setw(layout.rateWidth) << "pi" << ' '
                      << std::setw(layout.rateWidth) << "po" << ' '
                      << std::setw(layout.inWidth) << "in" << ' '
                      << std::setw(layout.rateWidth) << "sy" << ' '
                      << std::setw(layout.rateWidth) << "cs" << ' '
                      << std::setw(layout.cpuWidth) << "us" << ' '
                      << std::setw(layout.cpuWidth) << "sy" << ' '
                      << std::setw(layout.cpuWidth) << "id"
                      << "\n";
        }
    }

    static void PrintSummary(NtApiEngine& ntEngine, PdhFallbackEngine& pdhEngine) {
        SystemSample sample;
        if (!ntEngine.TakeSample(sample)) {
            std::cerr << "vmstat: failed to gather system performance counters.\n";
            return;
        }

        uint64_t pageSize = sample.psApiPerf.PageSize;
        uint64_t totalPhys = sample.psApiPerf.PhysicalTotal * pageSize;
        uint64_t freePhys  = sample.psApiPerf.PhysicalAvailable * pageSize;
        uint64_t commitTot = sample.psApiPerf.CommitTotal * pageSize;
        uint64_t commitLim = sample.psApiPerf.CommitLimit * pageSize;

        std::cout << std::setw(14) << sample.psApiPerf.ProcessCount  << " processes\n"
                  << std::setw(14) << sample.psApiPerf.ThreadCount   << " threads\n"
                  << std::setw(14) << (totalPhys / 1024)             << " KB total physical memory\n"
                  << std::setw(14) << (freePhys / 1024)              << " KB available physical memory\n"
                  << std::setw(14) << (commitTot / 1024)             << " KB committed virtual memory\n"
                  << std::setw(14) << (commitLim / 1024)             << " KB total commit limit\n";

        if (sample.ntPerfValid) {
            std::cout << std::setw(14) << sample.perfInfo.PageFaultCount       << " page faults\n"
                      << std::setw(14) << sample.perfInfo.PageReadCount        << " page read operations\n"
                      << std::setw(14) << sample.perfInfo.PagefilePagesWritten << " pages written to pagefile\n"
                      << std::setw(14) << sample.perfInfo.ContextSwitches      << " context switches\n"
                      << std::setw(14) << sample.perfInfo.SystemCalls          << " system calls\n";
        } else {
            PdhRateSample pdhRates;
            bool pdhAvailable = pdhEngine.CollectRates(pdhRates) && pdhRates.anyValid;
            if (pdhAvailable) {
                std::cout << std::setw(14) << (pdhRates.fltValid ? std::to_string(static_cast<long>(pdhRates.fltPerSec)) : std::string("N/A")) << " page faults/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.pagesInValid ? std::to_string(static_cast<long>(pdhRates.pagesInPerSec)) : std::string("N/A")) << " pages input/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.pagesOutValid ? std::to_string(static_cast<long>(pdhRates.pagesOutPerSec)) : std::string("N/A")) << " pages output/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.ctxSwitchValid ? std::to_string(static_cast<long>(pdhRates.ctxSwitchPerSec)) : std::string("N/A")) << " context switches/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.sysCallsValid ? std::to_string(static_cast<long>(pdhRates.sysCallsPerSec)) : std::string("N/A")) << " system calls/sec (PDH)\n";
            } else {
                std::cout << std::setw(14) << "N/A" << " page faults (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " page read operations (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " pages written to pagefile (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " context switches (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " system calls (NT/PDH counters unavailable)\n";
            }
        }
    }
};

// ============================================================================
// 6. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

static volatile bool g_running = true;
static std::condition_variable g_shutdownCV;
static std::mutex g_shutdownMutex;

BOOL WINAPI GlobalConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        g_running = false;
        g_shutdownCV.notify_all();
        return TRUE;
    }
    return FALSE;
}

class OptionParser {
public:
    static void ShowHelp() {
        std::cout << R"(vmstat(1)               CrossShell for UNIX Reference Manual                 vmstat(1)

    NAME
        vmstat - report virtual memory statistics

    SYNOPSIS
        vmstat [OPTIONS] [INTERVAL [COUNT]]

    DESCRIPTION
        Report information about processes, memory, paging, block IO, traps,
        and cpu activity over time.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

        -s, --summary
            Display event and memory summary counters and exit.

        -H, --human
            Display memory metrics in human-readable units (B/K/M/G).

        -M, --megabytes
            Display memory metrics in Megabytes (MB).

        -K, --kilobytes
            Display memory metrics in Kilobytes (KB) (default).

        -n, --lines NUMBER
            Repeat header output every NUMBER lines (default: 20).

        --output MODE
            Output mode: table, csv, json (default: table).

        --csv
            Shortcut for --output csv.

        --json
            Shortcut for --output json.

        --layout MODE
            Layout mode: auto, compact, single-line.

        --compact
            Force compact layout.

        --single-line
            Disable split-row compact layout.

    EXAMPLES
        vmstat 1 5
            Report every 1 second, 5 times.

        vmstat -H 2
            Human readable output updated every 2 seconds.

        vmstat --csv 1 5
            CSV output for automation.

        vmstat -s
            Display system memory summary.

    CrossShell for UNIX                                                      vmstat(1)
)";
    }

    static void ShowVersion() {
        std::cout << "vmstat v1.0.0\nCopyright (C) 2026\n\n";
    }

    bool Parse(int argc, wchar_t* argv[], VmstatOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = StringUtils::WStrToStr(argv[i]);
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                ShowHelp();
                exitEarly = true;
                return true;
            } else if (arg == "-v" || arg == "--version") {
                ShowVersion();
                exitEarly = true;
                return true;
            } else if (arg == "-s" || arg == "--summary") {
                opts.summaryMode = true;
            } else if (arg == "-H" || arg == "--human") {
                opts.mode = UnitMode::Human;
            } else if (arg == "-M" || arg == "--megabytes") {
                opts.mode = UnitMode::Megabytes;
            } else if (arg == "-K" || arg == "--kilobytes") {
                opts.mode = UnitMode::Kilobytes;
            } else if (arg == "--csv") {
                opts.outputMode = OutputMode::Csv;
            } else if (arg == "--json") {
                opts.outputMode = OutputMode::Json;
            } else if (arg == "--compact") {
                opts.layoutMode = LayoutMode::Compact;
            } else if (arg == "--single-line") {
                opts.layoutMode = LayoutMode::SingleLine;
            } else if (arg == "--output") {
                if (i + 1 >= argc) {
                    std::cerr << "vmstat: missing value for --output.\n";
                    return false;
                }
                std::string out = StringUtils::WStrToStr(argv[++i]);
                if (out == "table") opts.outputMode = OutputMode::Table;
                else if (out == "csv") opts.outputMode = OutputMode::Csv;
                else if (out == "json") opts.outputMode = OutputMode::Json;
                else {
                    std::cerr << "vmstat: invalid --output mode. Use table, csv, or json.\n";
                    return false;
                }
            } else if (arg == "--layout") {
                if (i + 1 >= argc) {
                    std::cerr << "vmstat: missing value for --layout.\n";
                    return false;
                }
                std::string lm = StringUtils::WStrToStr(argv[++i]);
                if (lm == "auto") opts.layoutMode = LayoutMode::Auto;
                else if (lm == "compact") opts.layoutMode = LayoutMode::Compact;
                else if (lm == "single-line") opts.layoutMode = LayoutMode::SingleLine;
                else {
                    std::cerr << "vmstat: invalid --layout mode. Use auto, compact, or single-line.\n";
                    return false;
                }
            } else if (arg == "-n" || arg == "--lines") {
                if (i + 1 < argc) {
                    std::string linesVal = StringUtils::WStrToStr(argv[++i]);
                    if (!StringUtils::TryParseInt(linesVal, 1, opts.headerInterval)) {
                        std::cerr << "vmstat: invalid value for -n/--lines. Expected integer >= 1.\n";
                        return false;
                    }
                } else {
                    std::cerr << "vmstat: missing value for -n/--lines.\n";
                    return false;
                }
            } else if (arg[0] != '-') {
                opts.positionalArgs.push_back(arg);
            } else {
                std::cerr << "vmstat: unknown option '" << arg << "'\nTry 'vmstat --help' for usage.\n";
                return false;
            }
        }

        if (!opts.positionalArgs.empty()) {
            if (!StringUtils::TryParseInt(opts.positionalArgs[0], 1, opts.interval)) {
                std::cerr << "vmstat: invalid interval '" << opts.positionalArgs[0] << "'. Expected integer >= 1.\n";
                return false;
            }
            if (opts.positionalArgs.size() > 1) {
                if (!StringUtils::TryParseInt(opts.positionalArgs[1], 1, opts.maxCount)) {
                    std::cerr << "vmstat: invalid count '" << opts.positionalArgs[1] << "'. Expected integer >= 1.\n";
                    return false;
                }
            }
            if (opts.positionalArgs.size() > 2) {
                std::cerr << "vmstat: too many positional arguments.\nTry 'vmstat --help' for usage.\n";
                return false;
            }
        }

        return true;
    }
};

class VmstatApplication {
private:
    OptionParser m_parser;
    NtApiEngine m_ntEngine;
    PdhFallbackEngine m_pdhEngine;

public:
    int Run(int argc, wchar_t* argv[]) {
        SetConsoleCtrlHandler(GlobalConsoleHandler, TRUE);

        bool ntApiInitialized = m_ntEngine.Init();
        bool pdhInitialized = m_pdhEngine.Init();

        auto finish = [&](int code) {
            m_pdhEngine.Shutdown();
            return code;
        };

        if (!ntApiInitialized) {
            std::cerr << "vmstat: NT performance counters unavailable; attempting PDH fallback.\n";
        }
        if (!pdhInitialized) {
            std::cerr << "vmstat: PDH performance counters unavailable; some rate fields may be N/A.\n";
        }

        VmstatOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return finish(1);
        }
        if (exitEarly) {
            return finish(0);
        }

        if (opts.summaryMode) {
            OutputFormatter::PrintSummary(m_ntEngine, m_pdhEngine);
            return finish(0);
        }

        SystemSample prevSample, currSample;
        if (!m_ntEngine.TakeSample(prevSample)) {
            std::cerr << "vmstat: failed to read system status.\n";
            return finish(1);
        }

        if (opts.interval <= 0) {
            opts.interval = 1;
            opts.maxCount = 1;
        }

        int printedLines = 0;
        int currentCount = 0;

        if (opts.outputMode == OutputMode::Table) {
            OutputFormatter::PrintHeader(opts.mode, opts.layoutMode);
            std::cerr << "vmstat: collecting baseline for " << opts.interval << " second(s)...\n";
        } else if (opts.outputMode == OutputMode::Csv) {
            std::cout << "sample,run,blk,thr,swpd,free,buff,cache,flt,pi,po,in,sy,cs,us,sy_cpu,id,src_flt,src_pi,src_po,src_in,src_sy,src_cs\n";
        }

        while (g_running) {
            {
                std::unique_lock<std::mutex> lock(g_shutdownMutex);
                if (g_shutdownCV.wait_for(lock, std::chrono::seconds(opts.interval), [] { return !g_running; })) {
                    break;
                }
            }
            if (!m_ntEngine.TakeSample(currSample)) break;

            double dt = std::chrono::duration<double>(currSample.timestamp - prevSample.timestamp).count();
            if (dt <= 0.001) dt = 1.0;

            ULONG activeProcs = currSample.psApiPerf.ProcessCount;
            ULONG totalThreads = currSample.psApiPerf.ThreadCount;
            ULONG blockedProcs = 0;

            uint64_t pageSize = currSample.psApiPerf.PageSize;
            uint64_t swpd  = currSample.psApiPerf.CommitTotal * pageSize;
            uint64_t freeM = currSample.psApiPerf.PhysicalAvailable * pageSize;
            uint64_t buff  = currSample.psApiPerf.KernelNonpaged * pageSize;
            uint64_t cache = (currSample.psApiPerf.KernelPaged + currSample.psApiPerf.SystemCache) * pageSize;

            const bool ntRateAvailable = prevSample.ntPerfValid && currSample.ntPerfValid;
            PdhRateSample pdhRates;
            (void)m_pdhEngine.CollectRates(pdhRates);

            double fltRate = 0.0;
            double piRate  = 0.0;
            double poRate  = 0.0;
            double inRate  = 0.0;
            double csRate  = 0.0;
            double syRate  = 0.0;
            bool fltAvailable = false;
            bool piAvailable = false;
            bool poAvailable = false;
            bool inAvailable = false;
            bool syAvailable = false;
            bool csAvailable = false;

            if (ntRateAvailable) {
                fltRate = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.PageFaultCount, prevSample.perfInfo.PageFaultCount, dt);
                piRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.PageReadCount, prevSample.perfInfo.PageReadCount, dt);
                poRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.PagefilePagesWritten, prevSample.perfInfo.PagefilePagesWritten, dt);
                csRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.ContextSwitches, prevSample.perfInfo.ContextSwitches, dt);
                syRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.SystemCalls, prevSample.perfInfo.SystemCalls, dt);
                fltAvailable = true;
                piAvailable = true;
                poAvailable = true;
                syAvailable = true;
                csAvailable = true;
            }

            if (!fltAvailable && pdhRates.fltValid) {
                fltRate = NtApiEngine::ClampRate(pdhRates.fltPerSec);
                fltAvailable = true;
            }
            if (!piAvailable && pdhRates.pagesInValid) {
                piRate  = NtApiEngine::ClampRate(pdhRates.pagesInPerSec);
                piAvailable = true;
            }
            if (!poAvailable && pdhRates.pagesOutValid) {
                poRate  = NtApiEngine::ClampRate(pdhRates.pagesOutPerSec);
                poAvailable = true;
            }
            if (pdhRates.interruptsValid) {
                inRate  = NtApiEngine::ClampRate(pdhRates.interruptsPerSec);
                inAvailable = true;
            }
            if (!csAvailable && pdhRates.ctxSwitchValid) {
                csRate  = NtApiEngine::ClampRate(pdhRates.ctxSwitchPerSec);
                csAvailable = true;
            }
            if (!syAvailable && pdhRates.sysCallsValid) {
                syRate  = NtApiEngine::ClampRate(pdhRates.sysCallsPerSec);
                syAvailable = true;
            }

            const bool fltFromNt = ntRateAvailable;
            const bool piFromNt = ntRateAvailable;
            const bool poFromNt = ntRateAvailable;
            const bool syFromNt = ntRateAvailable;
            const bool csFromNt = ntRateAvailable;
            const char* srcFlt = NtApiEngine::SelectSourceTag(fltAvailable && fltFromNt, fltAvailable && !fltFromNt);
            const char* srcPi = NtApiEngine::SelectSourceTag(piAvailable && piFromNt, piAvailable && !piFromNt);
            const char* srcPo = NtApiEngine::SelectSourceTag(poAvailable && poFromNt, poAvailable && !poFromNt);
            const char* srcIn = NtApiEngine::SelectSourceTag(false, inAvailable);
            const char* srcSy = NtApiEngine::SelectSourceTag(syAvailable && syFromNt, syAvailable && !syFromNt);
            const char* srcCs = NtApiEngine::SelectSourceTag(csAvailable && csFromNt, csAvailable && !csFromNt);

            ULONGLONG idleDelta   = currSample.idleTime.QuadPart - prevSample.idleTime.QuadPart;
            ULONGLONG kernelDelta = currSample.kernelTime.QuadPart - prevSample.kernelTime.QuadPart;
            ULONGLONG userDelta   = currSample.userTime.QuadPart - prevSample.userTime.QuadPart;

            ULONGLONG sysWorkDelta = (kernelDelta >= idleDelta) ? (kernelDelta - idleDelta) : 0;
            ULONGLONG totalCpuDelta = sysWorkDelta + userDelta + idleDelta;

            int usPct = (totalCpuDelta > 0) ? static_cast<int>(std::round((double)userDelta / totalCpuDelta * 100.0)) : 0;
            int syPct = (totalCpuDelta > 0) ? static_cast<int>(std::round((double)sysWorkDelta / totalCpuDelta * 100.0)) : 0;
            int idPct = (totalCpuDelta > 0) ? static_cast<int>(std::round((double)idleDelta / totalCpuDelta * 100.0)) : 100;

            if (opts.outputMode == OutputMode::Table) {
                if (printedLines > 0 && printedLines % opts.headerInterval == 0) {
                    OutputFormatter::PrintHeader(opts.mode, opts.layoutMode);
                }

                TableLayout layout = ConsoleManager::BuildTableLayout(ConsoleManager::GetConsoleWidth(), opts.layoutMode);

                if (layout.splitRows) {
                    std::cout << std::right
                              << std::setw(layout.runWidth) << activeProcs << ' '
                              << std::setw(layout.blkWidth) << blockedProcs << ' '
                              << std::setw(layout.thrWidth) << totalThreads << ' '
                              << StringUtils::FormatValue(swpd, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(freeM, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(buff, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(cache, opts.mode, layout.memWidth)
                              << "\n"
                              << StringUtils::FormatLongOrNA(fltAvailable, static_cast<long>(fltRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(piAvailable, static_cast<long>(piRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(poAvailable, static_cast<long>(poRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(inAvailable, static_cast<long>(inRate), layout.inWidth) << ' '
                              << StringUtils::FormatLongOrNA(syAvailable, static_cast<long>(syRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(csAvailable, static_cast<long>(csRate), layout.rateWidth) << ' '
                              << std::setw(layout.cpuWidth) << usPct << ' '
                              << std::setw(layout.cpuWidth) << syPct << ' '
                              << std::setw(layout.cpuWidth) << idPct
                              << "\n"
                              << "src flt=" << srcFlt << " pi=" << srcPi << " po=" << srcPo
                              << " in=" << srcIn << " sy=" << srcSy << " cs=" << srcCs << "\n";
                    printedLines += 3;
                } else {
                    std::cout << std::right
                              << std::setw(layout.runWidth) << activeProcs << ' '
                              << std::setw(layout.blkWidth) << blockedProcs << ' '
                              << std::setw(layout.thrWidth) << totalThreads << ' '
                              << StringUtils::FormatValue(swpd, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(freeM, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(buff, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(cache, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatLongOrNA(fltAvailable, static_cast<long>(fltRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(piAvailable, static_cast<long>(piRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(poAvailable, static_cast<long>(poRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(inAvailable, static_cast<long>(inRate), layout.inWidth) << ' '
                              << StringUtils::FormatLongOrNA(syAvailable, static_cast<long>(syRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(csAvailable, static_cast<long>(csRate), layout.rateWidth) << ' '
                              << std::setw(layout.cpuWidth) << usPct << ' '
                              << std::setw(layout.cpuWidth) << syPct << ' '
                              << std::setw(layout.cpuWidth) << idPct
                              << "\n"
                              << "src flt=" << srcFlt << " pi=" << srcPi << " po=" << srcPo
                              << " in=" << srcIn << " sy=" << srcSy << " cs=" << srcCs << "\n";
                    printedLines += 2;
                }
            } else if (opts.outputMode == OutputMode::Csv) {
                auto v = [](bool available, double value) -> std::string {
                    if (!available) return "N/A";
                    std::ostringstream ss;
                    ss << static_cast<long>(value);
                    return ss.str();
                };
                std::cout << (currentCount + 1) << ','
                          << activeProcs << ',' << blockedProcs << ',' << totalThreads << ','
                          << StringUtils::FormatValueRaw(swpd, opts.mode) << ',' << StringUtils::FormatValueRaw(freeM, opts.mode) << ','
                          << StringUtils::FormatValueRaw(buff, opts.mode) << ',' << StringUtils::FormatValueRaw(cache, opts.mode) << ','
                          << v(fltAvailable, fltRate) << ',' << v(piAvailable, piRate) << ','
                          << v(poAvailable, poRate) << ',' << v(inAvailable, inRate) << ','
                          << v(syAvailable, syRate) << ',' << v(csAvailable, csRate) << ','
                          << usPct << ',' << syPct << ',' << idPct << ','
                          << srcFlt << ',' << srcPi << ',' << srcPo << ',' << srcIn << ',' << srcSy << ',' << srcCs << '\n';
            } else {
                auto v = [](bool available, double value) -> std::string {
                    if (!available) return "null";
                    std::ostringstream ss;
                    ss << static_cast<long>(value);
                    return ss.str();
                };
                std::cout << "{\"sample\":" << (currentCount + 1)
                          << ",\"run\":" << activeProcs
                          << ",\"blk\":" << blockedProcs
                          << ",\"thr\":" << totalThreads
                          << ",\"swpd\":\"" << StringUtils::FormatValueRaw(swpd, opts.mode)
                          << "\",\"free\":\"" << StringUtils::FormatValueRaw(freeM, opts.mode)
                          << "\",\"buff\":\"" << StringUtils::FormatValueRaw(buff, opts.mode)
                          << "\",\"cache\":\"" << StringUtils::FormatValueRaw(cache, opts.mode)
                          << "\",\"flt\":" << v(fltAvailable, fltRate)
                          << ",\"pi\":" << v(piAvailable, piRate)
                          << ",\"po\":" << v(poAvailable, poRate)
                          << ",\"in\":" << v(inAvailable, inRate)
                          << ",\"sy\":" << v(syAvailable, syRate)
                          << ",\"cs\":" << v(csAvailable, csRate)
                          << ",\"us\":" << usPct
                          << ",\"sy_cpu\":" << syPct
                          << ",\"id\":" << idPct
                          << ",\"src_flt\":\"" << srcFlt
                          << "\",\"src_pi\":\"" << srcPi
                          << "\",\"src_po\":\"" << srcPo
                          << "\",\"src_in\":\"" << srcIn
                          << "\",\"src_sy\":\"" << srcSy
                          << "\",\"src_cs\":\"" << srcCs
                          << "\"}" << '\n';
            }

            currentCount++;
            prevSample = currSample;

            if (opts.maxCount > 0 && currentCount >= opts.maxCount) {
                break;
            }
        }

        return finish(0);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    VmstatApplication app;
    return app.Run(argc, argv);
}
