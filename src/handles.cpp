/*
 * Copyright (c) 2026 PC/OpenSystems LLC contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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
#include <tlhelp32.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <sstream>

// ============================================================================
// 1. RAII Wrappers & Utilities
// ============================================================================
class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle_(h) {}
    ~ScopedHandle() { close(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE get() const { return handle_; }
    bool isValid() const { return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr; }

    void close() {
        if (isValid()) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};

// ============================================================================
// 2. Data Model
// ============================================================================
struct ProcessHandleInfo {
    DWORD pid = 0;
    DWORD handleCount = 0;
    std::wstring name;
    bool accessDenied = false;

    // Helper to get narrow UTF-8 representation of the process name
    std::string narrowName() const {
        if (name.empty()) return "";
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), NULL, 0, NULL, NULL);
        std::string strTo(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), &strTo[0], sizeNeeded, NULL, NULL);
        return strTo;
    }
};

// ============================================================================
// 3. Formatter Abstraction (Strategy Pattern)
// ============================================================================
class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) = 0;
};

class TableFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        // Compute column widths
        size_t maxNameLen = 12; // "ProcessName" min length
        for (const auto& item : data) {
            maxNameLen = (std::max)(maxNameLen, item.narrowName().length());
        }

        // Header
        os << std::left 
           << std::setw(10) << "PID"
           << std::setw(14) << "Handles"
           << std::setw(maxNameLen) << "ProcessName" 
           << "\n";

        os << std::string(10 + 14 + maxNameLen, '-') << "\n";

        // Rows
        for (const auto& item : data) {
            os << std::left << std::setw(10) << item.pid;
            if (item.accessDenied) {
                os << std::left << std::setw(14) << "<AccessDenied>";
            } else {
                os << std::left << std::setw(14) << item.handleCount;
            }
            os << item.narrowName() << "\n";
        }
    }
};

class CsvFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        os << "PID,HandleCount,ProcessName,AccessDenied\n";
        for (const auto& item : data) {
            os << item.pid << ","
               << item.handleCount << ",\""
               << escapeQuotes(item.narrowName()) << "\","
               << (item.accessDenied ? "true" : "false") << "\n";
        }
    }

private:
    static std::string escapeQuotes(std::string str) {
        size_t pos = 0;
        while ((pos = str.find('\"', pos)) != std::string::npos) {
            str.replace(pos, 1, "\"\"");
            pos += 2;
        }
        return str;
    }
};

class JsonFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        os << "[\n";
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& item = data[i];
            os << "  {\n";
            os << "    \"pid\": " << item.pid << ",\n";
            os << "    \"handleCount\": " << item.handleCount << ",\n";
            os << "    \"processName\": \"" << escapeJson(item.narrowName()) << "\",\n";
            os << "    \"accessDenied\": " << (item.accessDenied ? "true" : "false") << "\n";
            os << "  }" << (i + 1 < data.size() ? "," : "") << "\n";
        }
        os << "]\n";
    }

private:
    static std::string escapeJson(const std::string& str) {
        std::ostringstream ss;
        for (char c : str) {
            if (c == '"') ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else if (c == '\b') ss << "\\b";
            else if (c == '\f') ss << "\\f";
            else if (c == '\n') ss << "\\n";
            else if (c == '\r') ss << "\\r";
            else if (c == '\t') ss << "\\t";
            else ss << c;
        }
        return ss.str();
    }
};

class PipeFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessHandleInfo>& data) override {
        os << "PID|HandleCount|ProcessName\n";
        for (const auto& item : data) {
            os << item.pid << "|"
               << (item.accessDenied ? 0 : item.handleCount) << "|"
               << item.narrowName() << "\n";
        }
    }
};

#include <winternl.h>

using NtQuerySystemInformationFn = NTSTATUS(NTAPI*)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

typedef LONG KPRIORITY;

struct SYSTEM_PROCESS_INFORMATION_ENTRY {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
};

// ============================================================================
// 4. Handle Collector Service
// ============================================================================
class HandleCollector {
public:
    static std::vector<ProcessHandleInfo> collect() {
        std::vector<ProcessHandleInfo> results;

        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return results;

        auto querySystem = reinterpret_cast<NtQuerySystemInformationFn>(
            GetProcAddress(ntdll, "NtQuerySystemInformation"));
        if (!querySystem) return results;

        ULONG size = 256 * 1024;
        std::vector<BYTE> buffer(size);
        ULONG returned = 0;
        while (querySystem(static_cast<SYSTEM_INFORMATION_CLASS>(5), buffer.data(), size, &returned) != 0) {
            size = (returned > size) ? (returned + 32768) : (size * 2);
            if (size > 64 * 1024 * 1024) return results;
            buffer.resize(size);
        }

        BYTE* p = buffer.data();
        while (p) {
            auto* spi = reinterpret_cast<SYSTEM_PROCESS_INFORMATION_ENTRY*>(p);
            ProcessHandleInfo info;
            info.pid = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(spi->UniqueProcessId));
            info.handleCount = spi->HandleCount;
            info.accessDenied = false;

            if (spi->ImageName.Buffer && spi->ImageName.Length > 0) {
                info.name.assign(spi->ImageName.Buffer, spi->ImageName.Length / sizeof(wchar_t));
            } else {
                if (info.pid == 0) info.name = L"System Idle Process";
                else if (info.pid == 4) info.name = L"System";
                else info.name = L"<unknown>";
            }

            results.push_back(info);

            if (spi->NextEntryOffset == 0) break;
            p += spi->NextEntryOffset;
        }

        return results;
    }
};


// ============================================================================
// 5. Configuration & Command Line Parser
// ============================================================================
enum class OutputType { Table, CSV, JSON, Pipe };
enum class SortColumn { Handles, PID, Name };

struct AppConfig {
    OutputType format = OutputType::Table;
    SortColumn sortBy = SortColumn::Handles;
    bool sortDescending = true;
    size_t topN = 0; // 0 = all
    std::wstring filterName = L"";
    DWORD filterPid = 0;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pipeCommand = L"";
};

class CommandLineParser {
public:
    static AppConfig parse(int argc, wchar_t* argv[]) {
        AppConfig config;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                config.showHelp = true;
                return config;
            } else if (arg == L"-V" || arg == L"--version") {
                config.showVersion = true;
                return config;
            } else if (arg == L"--json" || arg == L"-j") {
                config.format = OutputType::JSON;
            } else if (arg == L"--csv") {
                config.format = OutputType::CSV;
            } else if (arg == L"--table") {
                config.format = OutputType::Table;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                config.pipeCommand = argv[++i];
            } else if ((arg == L"-f" || arg == L"--format" || arg == L"--output") && i + 1 < argc) {
                std::wstring fmt = argv[++i];
                if (fmt == L"table") config.format = OutputType::Table;
                else if (fmt == L"csv") config.format = OutputType::CSV;
                else if (fmt == L"json") config.format = OutputType::JSON;
                else if (fmt == L"pipe") config.format = OutputType::Pipe;
            } else if ((arg == L"-s" || arg == L"--sort") && i + 1 < argc) {
                std::wstring s = argv[++i];
                if (s == L"handles") config.sortBy = SortColumn::Handles;
                else if (s == L"pid") config.sortBy = SortColumn::PID;
                else if (s == L"name") config.sortBy = SortColumn::Name;
            } else if (arg == L"--asc") {
                config.sortDescending = false;
            } else if (arg == L"--desc") {
                config.sortDescending = true;
            } else if ((arg == L"-n" || arg == L"--name") && i + 1 < argc) {
                config.filterName = argv[++i];
            } else if ((arg == L"-p" || arg == L"--pid") && i + 1 < argc) {
                config.filterPid = std::stoul(argv[++i]);
            } else if ((arg == L"-t" || arg == L"--top") && i + 1 < argc) {
                config.topN = std::stoul(argv[++i]);
            }
        }
        return config;
    }

    static void printHelp() {
        std::wcout << LR"(handles(1)              CrossShell for UNIX Reference Manual              handles(1)

    NAME
        handles - inspect and query process handle counts across running tasks

    SYNOPSIS
        handles [OPTIONS]
        handles [-f FORMAT] [-s COLUMN] [--asc|--desc] [-n NAME] [-p PID] [-t COUNT]

    DESCRIPTION
        handles queries the NT kernel diagnostic system to enumerate active
        processes and report their open handle allocations. It provides
        filtering by process name or identifier, sorting by handle consumption,
        and structured output serialization.

    OPTIONS
        -f, --format FORMAT
            Output serialization format: table, csv, json, or pipe.
            Default is table.

        -s, --sort COLUMN
            Sort process list by COLUMN: 'handles' (default), 'pid', or 'name'.

        --asc
            Sort records in ascending order.

        --desc
            Sort records in descending order (default).

        -n, --name STRING
            Filter processes matching the specified name substring.

        -p, --pid PID
            Filter by exact Process Identifier (PID).

        -t, --top N
            Limit results to the top N processes.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        handles
            Display handle table for all running processes sorted by handle count.

        handles -t 10
            Display the top 10 processes consuming the most handles.

        handles -n svchost
            List all svchost processes and their handle allocations.

        handles -p 1234 --json
            Query handle statistics for PID 1234 formatted as JSON.

        handles -f csv --asc -s name
            Export all processes in CSV format sorted alphabetically by name.

    CrossShell for UNIX                                                 handles(1)
)";
    }

    static void printVersion() {
        std::wcout << L"handles (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 PC/OpenSystems LLC contributors. All rights reserved.\n";
    }
};

// ============================================================================
// 6. Application Controller
// ============================================================================
class HandleCountApp {
public:
    explicit HandleCountApp(AppConfig config) : config_(std::move(config)) {}

    void run() {
        auto processes = HandleCollector::collect();

        // 1. Filter
        if (config_.filterPid != 0) {
            processes.erase(std::remove_if(processes.begin(), processes.end(),
                [this](const ProcessHandleInfo& item) {
                    return item.pid != config_.filterPid;
                }), processes.end());
        }

        if (!config_.filterName.empty()) {
            std::wstring needle = toLower(config_.filterName);
            processes.erase(std::remove_if(processes.begin(), processes.end(),
                [&needle](const ProcessHandleInfo& item) {
                    std::wstring hay = toLower(item.name);
                    return hay.find(needle) == std::wstring::npos;
                }), processes.end());
        }

        // 2. Sort
        std::sort(processes.begin(), processes.end(), [this](const ProcessHandleInfo& a, const ProcessHandleInfo& b) {
            bool result = false;
            switch (config_.sortBy) {
                case SortColumn::Handles: result = a.handleCount < b.handleCount; break;
                case SortColumn::PID:     result = a.pid < b.pid; break;
                case SortColumn::Name:    result = a.name < b.name; break;
            }
            return config_.sortDescending ? !result : result;
        });

        // 3. Top N
        if (config_.topN > 0 && config_.topN < processes.size()) {
            processes.resize(config_.topN);
        }

        // 4. Output
        std::unique_ptr<IOutputFormatter> formatter;
        switch (config_.format) {
            case OutputType::Table: formatter = std::make_unique<TableFormatter>(); break;
            case OutputType::CSV:   formatter = std::make_unique<CsvFormatter>(); break;
            case OutputType::JSON:  formatter = std::make_unique<JsonFormatter>(); break;
            case OutputType::Pipe:  formatter = std::make_unique<PipeFormatter>(); break;
        }

        if (!config_.pipeCommand.empty()) {
            FILE* pipe = _wpopen(config_.pipeCommand.c_str(), L"w");
            if (pipe) {
                std::ostringstream ss;
                formatter->format(ss, processes);
                std::string s = ss.str();
                std::fwrite(s.data(), 1, s.size(), pipe);
                _pclose(pipe);
                return;
            }
        }

        formatter->format(std::cout, processes);
    }

private:
    AppConfig config_;

    static std::wstring toLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
    }
};

// ============================================================================
// Entry Point
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    AppConfig config = CommandLineParser::parse(argc, argv);

    if (config.showHelp) {
        CommandLineParser::printHelp();
        return 0;
    }

    if (config.showVersion) {
        CommandLineParser::printVersion();
        return 0;
    }

    HandleCountApp app(config);
    app.run();
    return 0;
}