/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

// ============================================================================
// 1. DOMAIN MODELS
// ============================================================================
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

// ============================================================================
// 2. CONFIGURATION & QUALIFIERS
// ============================================================================
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

// ============================================================================
// 3. HARDWARE & KERNEL METRIC COLLECTOR
// ============================================================================
class MemoryMetricsCollector {
public:
    static MemorySnapshot Collect() {
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

private:
    static BOOL CALLBACK PageFileCallback(LPVOID pContext, PENUM_PAGE_FILE_INFORMATION pInfo, LPCWSTR lpFileName) {
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
};

// ============================================================================
// 4. FORMATTERS (Table, JSON, CSV)
// ============================================================================
class OutputFormatter {
protected:
    std::string FormatBytes(uint64_t bytes, DisplayUnit unit) const {
        if (unit == DisplayUnit::Bytes) return std::to_string(bytes) + " B";
        
        double val = static_cast<double>(bytes);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);

        switch (unit) {
            case DisplayUnit::Kibi: oss << (val / 1024.0) << " KiB"; return oss.str();
            case DisplayUnit::Mebi: oss << (val / (1024.0 * 1024.0)) << " MiB"; return oss.str();
            case DisplayUnit::Gibi: oss << (val / (1024.0 * 1024.0 * 1024.0)) << " GiB"; return oss.str();
            case DisplayUnit::Human:
            default:
                if (val >= 1024.0 * 1024.0 * 1024.0) {
                    oss << (val / (1024.0 * 1024.0 * 1024.0)) << " GiB";
                } else if (val >= 1024.0 * 1024.0) {
                    oss << (val / (1024.0 * 1024.0)) << " MiB";
                } else if (val >= 1024.0) {
                    oss << (val / 1024.0) << " KiB";
                } else {
                    oss << val << " B";
                }
                return oss.str();
        }
    }

    double Percent(uint64_t part, uint64_t total) const {
        return total == 0 ? 0.0 : (static_cast<double>(part) / total) * 100.0;
    }

    std::string Narrow(const std::wstring& wide) const {
        if (wide.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), &result[0], size, nullptr, nullptr);
        return result;
    }

public:
    virtual ~OutputFormatter() = default;
    virtual void Render(std::ostream& os, const MemorySnapshot& snap, const Config& cfg) = 0;
};

// --- TABLE FORMATTER ---
class TableFormatter : public OutputFormatter {
public:
    void Render(std::ostream& os, const MemorySnapshot& s, const Config& cfg) override {
        os << "\nPhysical Memory (RAM)\n\n";
        os << std::left << std::setw(17) << "Total"
           << std::setw(16) << "Used"
           << std::setw(16) << "System Cache"
           << std::setw(16) << "Available"
           << "PCT Used\n";

        os << std::left << std::setw(17) << FormatBytes(s.totalPhysical, cfg.unit)
           << std::setw(16) << FormatBytes(s.usedPhysical, cfg.unit)
           << std::setw(16) << FormatBytes(s.systemCache, cfg.unit)
           << std::setw(16) << FormatBytes(s.availPhysical, cfg.unit)
           << std::fixed << std::setprecision(2) << Percent(s.usedPhysical, s.totalPhysical) << "%\n\n";

        os << "\nCommit Charge (Virtual Swap)\n\n";
        os << std::left << std::setw(17) << "Limit"
           << std::setw(16) << "Committed"
           << std::setw(16) << "Peak Committed"
           << std::setw(16) << "Available"
           << "PCT Used\n";

        uint64_t commitAvail = (s.commitLimit > s.commitTotal) ? (s.commitLimit - s.commitTotal) : 0;
        os << std::left << std::setw(17) << FormatBytes(s.commitLimit, cfg.unit)
           << std::setw(16) << FormatBytes(s.commitTotal, cfg.unit)
           << std::setw(16) << FormatBytes(s.commitPeak, cfg.unit)
           << std::setw(16) << FormatBytes(commitAvail, cfg.unit)
           << std::fixed << std::setprecision(2) << Percent(s.commitTotal, s.commitLimit) << "%\n\n";

        if (cfg.showKernelDetails) {
            os << "\nKernel Allocations\n\n";
            os << std::left << std::setw(25) << "Paged Pool:" << FormatBytes(s.kernelPaged, cfg.unit) << "\n"
               << std::left << std::setw(25) << "Non-Paged Pool:" << FormatBytes(s.kernelNonPaged, cfg.unit) << "\n\n";
        }

        if (cfg.showPagefileDetails) {
            os << "\nPaging File Topology\n\n";
            os << std::left << std::setw(32) << "DEVICE/PATH"
               << std::setw(16) << "TOTAL"
               << std::setw(16) << "USED"
               << std::setw(16) << "FREE"
               << "PCT USED\n";

            uint64_t totalPF = 0, usedPF = 0;
            for (const auto& pf : s.pageFiles) {
                totalPF += pf.totalBytes;
                usedPF  += pf.usedBytes;
                uint64_t freeBytes = (pf.totalBytes > pf.usedBytes) ? (pf.totalBytes - pf.usedBytes) : 0;

                os << std::left << std::setw(32) << Narrow(pf.path)
                   << std::setw(16) << FormatBytes(pf.totalBytes, cfg.unit)
                   << std::setw(16) << FormatBytes(pf.usedBytes, cfg.unit)
                   << std::setw(16) << FormatBytes(freeBytes, cfg.unit)
                   << std::fixed << std::setprecision(2) << Percent(pf.usedBytes, pf.totalBytes) << "%\n";
            }
            os << "---------------------------------------------------------------------------------------\n";
            uint64_t totalFree = (totalPF > usedPF) ? (totalPF - usedPF) : 0;
            os << std::left << std::setw(32) << "Total Pagefile Capacity"
               << std::setw(16) << FormatBytes(totalPF, cfg.unit)
               << std::setw(16) << FormatBytes(usedPF, cfg.unit)
               << std::setw(16) << FormatBytes(totalFree, cfg.unit)
               << std::fixed << std::setprecision(2) << Percent(usedPF, totalPF) << "%\n\n";
        }
    }
};

// --- JSON FORMATTER ---
class JsonFormatter : public OutputFormatter {
public:
    void Render(std::ostream& os, const MemorySnapshot& s, const Config&) override {
        os << "{\n"
           << "  \"physical_memory\": {\n"
           << "    \"total_bytes\": " << s.totalPhysical << ",\n"
           << "    \"used_bytes\": " << s.usedPhysical << ",\n"
           << "    \"system_cache_bytes\": " << s.systemCache << ",\n"
           << "    \"available_bytes\": " << s.availPhysical << ",\n"
           << "    \"percent_used\": " << std::fixed << std::setprecision(2) << Percent(s.usedPhysical, s.totalPhysical) << "\n"
           << "  },\n"
           << "  \"commit_charge\": {\n"
           << "    \"limit_bytes\": " << s.commitLimit << ",\n"
           << "    \"committed_bytes\": " << s.commitTotal << ",\n"
           << "    \"peak_bytes\": " << s.commitPeak << ",\n"
           << "    \"available_bytes\": " << ((s.commitLimit > s.commitTotal) ? (s.commitLimit - s.commitTotal) : 0) << ",\n"
           << "    \"percent_used\": " << std::fixed << std::setprecision(2) << Percent(s.commitTotal, s.commitLimit) << "\n"
           << "  },\n"
           << "  \"kernel_memory\": {\n"
           << "    \"paged_pool_bytes\": " << s.kernelPaged << ",\n"
           << "    \"non_paged_pool_bytes\": " << s.kernelNonPaged << "\n"
           << "  },\n"
           << "  \"paging_devices\": [\n";

        for (size_t i = 0; i < s.pageFiles.size(); ++i) {
            const auto& pf = s.pageFiles[i];
            uint64_t freeBytes = (pf.totalBytes > pf.usedBytes) ? (pf.totalBytes - pf.usedBytes) : 0;
            std::string escapedPath;
            for (char c : Narrow(pf.path)) {
                if (c == '\\') escapedPath += "\\\\";
                else escapedPath += c;
            }

            os << "    {\n"
               << "      \"path\": \"" << escapedPath << "\",\n"
               << "      \"total_bytes\": " << pf.totalBytes << ",\n"
               << "      \"used_bytes\": " << pf.usedBytes << ",\n"
               << "      \"free_bytes\": " << freeBytes << ",\n"
               << "      \"percent_used\": " << std::fixed << std::setprecision(2) << Percent(pf.usedBytes, pf.totalBytes) << "\n"
               << "    }" << (i + 1 < s.pageFiles.size() ? "," : "") << "\n";
        }
        os << "  ]\n}\n";
    }
};

// --- CSV FORMATTER ---
class CsvFormatter : public OutputFormatter {
public:
    void Render(std::ostream& os, const MemorySnapshot& s, const Config&) override {
        os << "Category,SubItem,TotalBytes,UsedBytes,FreeOrAvailBytes,PercentUsed\n";
        os << "RAM,Physical," << s.totalPhysical << "," << s.usedPhysical << "," << s.availPhysical << ","
           << std::fixed << std::setprecision(2) << Percent(s.usedPhysical, s.totalPhysical) << "\n";
        os << "RAM,SystemCache," << s.systemCache << "," << s.systemCache << ",0,100.00\n";
        os << "Commit,VirtualLimit," << s.commitLimit << "," << s.commitTotal << "," 
           << ((s.commitLimit > s.commitTotal) ? (s.commitLimit - s.commitTotal) : 0) << ","
           << std::fixed << std::setprecision(2) << Percent(s.commitTotal, s.commitLimit) << "\n";
        os << "Kernel,Paged," << s.kernelPaged << "," << s.kernelPaged << ",0,100.00\n";
        os << "Kernel,NonPaged," << s.kernelNonPaged << "," << s.kernelNonPaged << ",0,100.00\n";

        for (const auto& pf : s.pageFiles) {
            uint64_t freeBytes = (pf.totalBytes > pf.usedBytes) ? (pf.totalBytes - pf.usedBytes) : 0;
            os << "Pagefile,\"" << Narrow(pf.path) << "\"," << pf.totalBytes << "," << pf.usedBytes << "," << freeBytes << ","
               << std::fixed << std::setprecision(2) << Percent(pf.usedBytes, pf.totalBytes) << "\n";
        }
    }
};

// ============================================================================
// 5. COMMAND LINE PARSER
// ============================================================================
class CommandLineParser {
public:
    static Config Parse(int argc, char* argv[]) {
        Config cfg;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--human")        cfg.unit = DisplayUnit::Human;
            else if (arg == "-b" || arg == "--bytes")   cfg.unit = DisplayUnit::Bytes;
            else if (arg == "-k" || arg == "--kibi")    cfg.unit = DisplayUnit::Kibi;
            else if (arg == "-m" || arg == "--mebi")    cfg.unit = DisplayUnit::Mebi;
            else if (arg == "-g" || arg == "--gibi")    cfg.unit = DisplayUnit::Gibi;
            else if (arg == "--json")                   cfg.format = OutputFormat::Json;
            else if (arg == "--csv")                    cfg.format = OutputFormat::Csv;
            else if (arg == "--table")                  cfg.format = OutputFormat::Table;
            else if (arg == "-d" || arg == "--devices") cfg.showPagefileDetails = true;
            else if (arg == "-w" || arg == "--wide")    { cfg.showKernelDetails = true; cfg.showPagefileDetails = true; }
            else if (arg == "-s" || arg == "--seconds") {
                if (i + 1 < argc) cfg.repeatSeconds = std::stoi(argv[++i]);
            }
            else if (arg == "-c" || arg == "--count") {
                if (i + 1 < argc) cfg.maxCount = std::stoi(argv[++i]);
            }
            else if (arg == "/?" || arg == "--help")    cfg.showHelp = true;
            else if (arg == "-v" || arg == "--version") cfg.showVersion = true;
            else {
                std::cerr << "Unknown option: " << arg << " (Run meminfo --help for usage)\n";
            }
        }
        return cfg;
    }

    static void PrintHelp() {
        std::cout <<
R"(meminfo - Physical and Virtual Swap Memory Reporter

USAGE:
  meminfo [QUALIFIERS]

DISPLAY UNITS:
  -h, --human        Show scaling human-readable format (e.g., 16.24 GiB) [Default]
  -b, --bytes        Show outputs in raw bytes
  -k, --kibi         Show outputs in KiB (1024 bytes)
  -m, --mebi         Show outputs in MiB (1048576 bytes)
  -g, --gibi         Show outputs in GiB (1073741824 bytes)

OUTPUT FORMATS (Pipeline friendly):
  --table            Standard ASCII grid output [Default]
  --json             Serialize snapshot as JSON (raw byte counts)
  --csv              Emit comma-separated values

TOPOLOGY & DETAILS:
  -d, --devices      Show per-device pagefile breakdown (like HP-UX swapinfo)
  -w, --wide         Show extended kernel pools (Paged/Non-Paged) and devices

MONITORING & INTERVALS:
  -s, --seconds <N>  Repeat report every N seconds
  -c, --count <N>    Exit after N iterations (used alongside -s)

INFORMATIONAL:
  -?, --help         Print this comprehensive help screen
  -v, --version      Display application version
)";
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================
class MemInfoApp {
public:
    int Run(int argc, char* argv[]) {
        Config cfg = CommandLineParser::Parse(argc, argv);

        if (cfg.showHelp) {
            CommandLineParser::PrintHelp();
            return 0;
        }

        if (cfg.showVersion) {
            std::cout << "meminfo version 1.0.0\n";
            return 0;
        }

        std::unique_ptr<OutputFormatter> formatter;
        switch (cfg.format) {
            case OutputFormat::Json:  formatter = std::make_unique<JsonFormatter>(); break;
            case OutputFormat::Csv:   formatter = std::make_unique<CsvFormatter>(); break;
            case OutputFormat::Table:
            default:                  formatter = std::make_unique<TableFormatter>(); break;
        }

        int iterations = 0;
        while (true) {
            MemorySnapshot snap = MemoryMetricsCollector::Collect();
            formatter->Render(std::cout, snap, cfg);
            std::cout.flush();

            iterations++;
            if (cfg.repeatSeconds <= 0 || (cfg.maxCount > 0 && iterations >= cfg.maxCount)) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(cfg.repeatSeconds));
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    MemInfoApp app;
    return app.Run(argc, argv);
}