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

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <fcntl.h>
#include <iostream>
#include <io.h>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <chrono>
#include <thread>
#include <functional>

#pragma comment(lib, "pdh.lib")

// ============================================================================
// 1. DATA STRUCTURES & CONFIGURATION
// ============================================================================

struct DiskStats {
    double reads_sec = 0.0;
    double writes_sec = 0.0;
    double read_bytes_sec = 0.0;
    double write_bytes_sec = 0.0;
    double pct_disk_time = 0.0;
    double queue_length = 0.0;
};

struct IostatOptions {
    int interval = 0;
    int count = -1;
    double scale = 1024.0;
    std::string unit = "kB";
    bool saw_interval = false;
    bool saw_count = false;
};

// ============================================================================
// 2. STRING CONVERTER & HELPERS
// ============================================================================

class StringUtils {
public:
    static std::string WideToNarrow(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }
};

// ============================================================================
// 3. PDH COUNTER ENGINE
// ============================================================================

class PdhCounterEngine {
private:
    PDH_HQUERY m_hQuery = NULL;
    PDH_HCOUNTER m_hCpuUser = NULL;
    PDH_HCOUNTER m_hCpuSys = NULL;
    PDH_HCOUNTER m_hCpuIdle = NULL;
    PDH_HCOUNTER m_hReadsSec = NULL;
    PDH_HCOUNTER m_hWritesSec = NULL;
    PDH_HCOUNTER m_hReadBytesSec = NULL;
    PDH_HCOUNTER m_hWriteBytesSec = NULL;
    PDH_HCOUNTER m_hPctDiskTime = NULL;
    PDH_HCOUNTER m_hQueueLength = NULL;

    static PDH_HCOUNTER AddCounter(PDH_HQUERY hQuery, const std::wstring& path) {
        PDH_HCOUNTER hCounter = NULL;
        PDH_STATUS status = PdhAddEnglishCounterW(hQuery, path.c_str(), 0, &hCounter);
        if (status != ERROR_SUCCESS) {
            std::wcerr << L"Error: Failed to register counter: " << path 
                       << L" (Status code: 0x" << std::hex << status << L")\n";
        }
        return hCounter;
    }

    static void FetchDiskCounterArray(PDH_HCOUNTER hCounter, std::function<void(const std::wstring&, double)> assign_fn) {
        DWORD dwBufferSize = 0;
        DWORD dwItemCount = 0;
        PDH_STATUS status = PdhGetFormattedCounterArrayW(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
        
        if (status == PDH_MORE_DATA) {
            std::vector<BYTE> buffer(dwBufferSize);
            PDH_FMT_COUNTERVALUE_ITEM_W* pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
            status = PdhGetFormattedCounterArrayW(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
            
            if (status == ERROR_SUCCESS) {
                for (DWORD i = 0; i < dwItemCount; ++i) {
                    if (pItems[i].szName) {
                        assign_fn(pItems[i].szName, pItems[i].FmtValue.doubleValue);
                    }
                }
            }
        }
    }

public:
    PdhCounterEngine() = default;

    ~PdhCounterEngine() {
        Close();
    }

    bool Initialize() {
        if (PdhOpenQueryW(NULL, 0, &m_hQuery) != ERROR_SUCCESS) {
            std::cerr << "Fatal Error: Failed to initialize PDH Query engine.\n";
            return false;
        }

        m_hCpuUser = AddCounter(m_hQuery, L"\\Processor(_Total)\\% User Time");
        m_hCpuSys = AddCounter(m_hQuery, L"\\Processor(_Total)\\% Privileged Time");
        m_hCpuIdle = AddCounter(m_hQuery, L"\\Processor(_Total)\\% Idle Time");

        m_hReadsSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Reads/sec");
        m_hWritesSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Writes/sec");
        m_hReadBytesSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Read Bytes/sec");
        m_hWriteBytesSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Write Bytes/sec");
        m_hPctDiskTime = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\% Disk Time");
        m_hQueueLength = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Current Disk Queue Length");

        if (!m_hCpuUser || !m_hCpuSys || !m_hCpuIdle || !m_hReadsSec || !m_hWritesSec || 
            !m_hReadBytesSec || !m_hWriteBytesSec || !m_hPctDiskTime || !m_hQueueLength) {
            std::cerr << "Fatal Error: One or more critical performance counters could not be resolved.\n";
            Close();
            return false;
        }

        PdhCollectQueryData(m_hQuery);
        return true;
    }

    bool CollectSample(double& userPct, double& sysPct, double& idlePct, std::map<std::wstring, DiskStats>& diskMap) {
        PDH_STATUS status = PdhCollectQueryData(m_hQuery);
        if (status != ERROR_SUCCESS) {
            std::cerr << "Error collecting query data. Status code: 0x" << std::hex << status << "\n";
            return false;
        }

        userPct = 0.0;
        sysPct = 0.0;
        idlePct = 0.0;
        PDH_FMT_COUNTERVALUE cvUser{}, cvSys{}, cvIdle{};

        if (PdhGetFormattedCounterValue(m_hCpuUser, PDH_FMT_DOUBLE, NULL, &cvUser) == ERROR_SUCCESS) {
            userPct = cvUser.doubleValue;
        }
        if (PdhGetFormattedCounterValue(m_hCpuSys, PDH_FMT_DOUBLE, NULL, &cvSys) == ERROR_SUCCESS) {
            sysPct = cvSys.doubleValue;
        }
        if (PdhGetFormattedCounterValue(m_hCpuIdle, PDH_FMT_DOUBLE, NULL, &cvIdle) == ERROR_SUCCESS) {
            idlePct = cvIdle.doubleValue;
        }

        diskMap.clear();
        FetchDiskCounterArray(m_hReadsSec, [&](const std::wstring& name, double val) { diskMap[name].reads_sec = val; });
        FetchDiskCounterArray(m_hWritesSec, [&](const std::wstring& name, double val) { diskMap[name].writes_sec = val; });
        FetchDiskCounterArray(m_hReadBytesSec, [&](const std::wstring& name, double val) { diskMap[name].read_bytes_sec = val; });
        FetchDiskCounterArray(m_hWriteBytesSec, [&](const std::wstring& name, double val) { diskMap[name].write_bytes_sec = val; });
        FetchDiskCounterArray(m_hPctDiskTime, [&](const std::wstring& name, double val) { diskMap[name].pct_disk_time = val; });
        FetchDiskCounterArray(m_hQueueLength, [&](const std::wstring& name, double val) { diskMap[name].queue_length = val; });

        return true;
    }

    void Close() {
        if (m_hQuery) {
            PdhCloseQuery(m_hQuery);
            m_hQuery = NULL;
        }
    }
};

// ============================================================================
// 4. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void PrintUsage() {
        std::cout << R"(iostat(1)          CrossShell for UNIX Reference Manual                iostat(1)

    NAME
        iostat - report CPU and physical disk I/O statistics

    SYNOPSIS
        iostat [OPTIONS] [INTERVAL] [COUNT]

    DESCRIPTION
        iostat queries Windows PDH performance counters to report CPU utilization
        percentages and physical disk throughput and queue metrics. If no interval
        is specified, a single snapshot report is generated.

    OPTIONS
        -c, --count COUNT
            Number of reports to generate before exiting.

        -n, --interval SECONDS
            Delay in seconds between successive reports.

        -k, --kilobytes
            Display disk throughput in kilobytes per second (default).

        -m, --megabytes
            Display disk throughput in megabytes per second.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        iostat
            Display single snapshot of CPU and disk activity.

        iostat 2 5
            Report statistics every 2 seconds for 5 iterations.

        iostat -m 1 10
            Display throughput in MB/s every second 10 times.

    CrossShell for UNIX                                                   iostat(1)
)";
    }

    static void PrintVersion() {
        std::cout << "iostat v1.0.0\n";
    }

    static void PrintCpuMetrics(double userPct, double sysPct, double idlePct) {
        std::cout << "\navg-cpu:  %user   %sys   %idle\n";
        std::cout << "          " 
                  << std::setw(5) << std::fixed << std::setprecision(2) << userPct << "  "
                  << std::setw(5) << sysPct << "  "
                  << std::setw(5) << idlePct << "\n\n";
    }

    static void PrintDiskTable(const std::map<std::wstring, DiskStats>& diskMap, double scale) {
        std::vector<std::pair<std::wstring, DiskStats>> individual_disks;
        std::pair<std::wstring, DiskStats> total_disk;
        bool has_total = false;

        for (auto const& pair : diskMap) {
            if (pair.first == L"_Total") {
                total_disk = pair;
                has_total = true;
            } else {
                individual_disks.push_back(pair);
            }
        }

        std::cout << std::left << std::setw(22) << "Device"
                  << std::right << std::setw(10) << "tps"
                  << std::setw(14) << "kB_read/s"
                  << std::setw(14) << "kB_wrtn/s"
                  << std::setw(10) << "aqu-sz"
                  << std::setw(10) << "%util" << "\n";

        auto printRow = [&](const std::wstring& wname, const DiskStats& stats) {
            std::string name = StringUtils::WideToNarrow(wname);
            double tps = stats.reads_sec + stats.writes_sec;
            double throughput_read = stats.read_bytes_sec / scale;
            double throughput_write = stats.write_bytes_sec / scale;
            double aqu_sz = stats.queue_length;
            double util = stats.pct_disk_time;

            std::cout << std::left << std::setw(22) << name
                      << std::right << std::setw(10) << std::fixed << std::setprecision(2) << tps
                      << std::setw(14) << throughput_read
                      << std::setw(14) << throughput_write
                      << std::setw(10) << aqu_sz
                      << std::setw(10) << util << "\n";
        };

        for (const auto& disk : individual_disks) {
            printRow(disk.first, disk.second);
        }

        if (has_total) {
            std::cout << std::string(80, '-') << "\n";
            printRow(total_disk.first, total_disk.second);
        }
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    bool Parse(int argc, char* argv[], IostatOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    if (!opts.saw_interval) {
                        try { opts.interval = std::stoi(argv[i]); if (opts.interval < 1) opts.interval = 1; opts.saw_interval = true; }
                        catch (...) { std::cerr << "iostat: invalid interval: " << argv[i] << "\n"; return false; }
                    } else if (!opts.saw_count) {
                        try { opts.count = std::stoi(argv[i]); if (opts.count < 1) opts.count = 1; opts.saw_count = true; }
                        catch (...) { std::cerr << "iostat: invalid count: " << argv[i] << "\n"; return false; }
                    } else {
                        std::cerr << "iostat: too many arguments\n";
                        return false;
                    }
                }
                break;
            }

            if (arg == "-h" || arg == "--help") {
                OutputFormatter::PrintUsage();
                exitEarly = true;
                return true;
            }
            if (arg == "-V" || arg == "--version") {
                OutputFormatter::PrintVersion();
                exitEarly = true;
                return true;
            }
            if (arg == "-k" || arg == "--kilobytes") {
                opts.scale = 1024.0;
                opts.unit = "kB";
            } else if (arg == "-m" || arg == "--megabytes") {
                opts.scale = 1024.0 * 1024.0;
                opts.unit = "MB";
            } else if (arg == "-n" || arg == "--interval") {
                if (i + 1 >= argc) {
                    std::cerr << "iostat: option requires an argument -- n\n";
                    return false;
                }
                try { opts.interval = std::stoi(argv[++i]); if (opts.interval < 1) opts.interval = 1; opts.saw_interval = true; }
                catch (...) { std::cerr << "iostat: invalid interval: " << argv[i] << "\n"; return false; }
            } else if (arg == "-c" || arg == "--count") {
                if (i + 1 >= argc) {
                    std::cerr << "iostat: option requires an argument -- c\n";
                    return false;
                }
                try { opts.count = std::stoi(argv[++i]); if (opts.count < 1) opts.count = 1; opts.saw_count = true; }
                catch (...) { std::cerr << "iostat: invalid count: " << argv[i] << "\n"; return false; }
            } else if (arg[0] == '-') {
                std::cerr << "iostat: unrecognized option: " << arg << "\n";
                OutputFormatter::PrintUsage();
                return false;
            } else {
                if (!opts.saw_interval) {
                    try { opts.interval = std::stoi(arg); if (opts.interval < 1) opts.interval = 1; opts.saw_interval = true; }
                    catch (...) { std::cerr << "iostat: invalid interval: " << arg << "\n"; return false; }
                } else if (!opts.saw_count) {
                    try { opts.count = std::stoi(arg); if (opts.count < 1) opts.count = 1; opts.saw_count = true; }
                    catch (...) { std::cerr << "iostat: invalid count: " << arg << "\n"; return false; }
                } else {
                    std::cerr << "iostat: too many arguments\n";
                    return false;
                }
            }
        }
        return true;
    }
};

class IostatApplication {
private:
    OptionParser m_parser;
    PdhCounterEngine m_engine;

public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        IostatOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        if (!m_engine.Initialize()) {
            return 1;
        }

        std::cout << "\n";
        std::cout << "Collecting metrics... Use Ctrl+C to terminate.\n";

        int iterations = 0;
        while (true) {
            if (opts.count != -1 && iterations >= opts.count) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(opts.interval == 0 ? 1 : opts.interval));

            double userPct = 0.0, sysPct = 0.0, idlePct = 0.0;
            std::map<std::wstring, DiskStats> diskMap;

            if (!m_engine.CollectSample(userPct, sysPct, idlePct, diskMap)) {
                break;
            }

            OutputFormatter::PrintCpuMetrics(userPct, sysPct, idlePct);
            OutputFormatter::PrintDiskTable(diskMap, opts.scale);

            iterations++;
            if (opts.interval == 0) {
                break;
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    IostatApplication app;
    return app.Run(argc, argv);
}
