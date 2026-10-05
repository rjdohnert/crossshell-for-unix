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
// 1. RAII Resource Management
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
struct ProcessThreadInfo {
    DWORD pid = 0;
    DWORD threadCount = 0;
    std::wstring name;

    // Converts wide process name into a UTF-8 standard string
    std::string narrowName() const {
        if (name.empty()) return "";
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), nullptr, 0, nullptr, nullptr);
        std::string strTo(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), &strTo[0], sizeNeeded, nullptr, nullptr);
        return strTo;
    }
};

// ============================================================================
// 3. Formatter Abstraction (Strategy Pattern)
// ============================================================================
class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) = 0;
};

class TableFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) override {
        size_t maxNameLen = 12; // Base length for "ProcessName"
        for (const auto& item : data) {
            maxNameLen = (std::max)(maxNameLen, item.narrowName().length());
        }

        // Table Header
        os << std::left 
           << std::setw(10) << "PID"
           << std::setw(14) << "Threads"
           << std::setw(maxNameLen) << "ProcessName" 
           << "\n";

        os << std::string(10 + 14 + maxNameLen, '-') << "\n";

        // Rows
        for (const auto& item : data) {
            os << std::left 
               << std::setw(10) << item.pid
               << std::setw(14) << item.threadCount
               << item.narrowName() << "\n";
        }
    }
};

class CsvFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) override {
        os << "PID,ThreadCount,ProcessName\n";
        for (const auto& item : data) {
            os << item.pid << ","
               << item.threadCount << ",\""
               << escapeQuotes(item.narrowName()) << "\"\n";
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
    void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) override {
        os << "[\n";
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& item = data[i];
            os << "  {\n";
            os << "    \"pid\": " << item.pid << ",\n";
            os << "    \"threadCount\": " << item.threadCount << ",\n";
            os << "    \"processName\": \"" << escapeJson(item.narrowName()) << "\"\n";
            os << "  }" << (i + 1 < data.size() ? "," : "") << "\n";
        }
        os << "]\n";
    }

private:
    static std::string escapeJson(const std::string& str) {
        std::ostringstream ss;
        for (char c : str) {
            switch (c) {
                case '\"': ss << "\\\""; break;
                case '\\': ss << "\\\\"; break;
                case '\b': ss << "\\b"; break;
                case '\f': ss << "\\f"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default:   ss << c; break;
            }
        }
        return ss.str();
    }
};

class PipeFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) override {
        os << "PID|ThreadCount|ProcessName\n";
        for (const auto& item : data) {
            os << item.pid << "|"
               << item.threadCount << "|"
               << item.narrowName() << "\n";
        }
    }
};

// ============================================================================
// 4. Thread Collector Service
// ============================================================================
class ThreadCollector {
public:
    static std::vector<ProcessThreadInfo> collect() {
        std::vector<ProcessThreadInfo> results;

        ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!snapshot.isValid()) {
            return results;
        }

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (!Process32FirstW(snapshot.get(), &pe32)) {
            return results;
        }

        do {
            ProcessThreadInfo info;
            info.pid = pe32.th32ProcessID;
            info.threadCount = pe32.cntThreads; // Populated by OS snapshot
            info.name = pe32.szExeFile;

            results.push_back(info);
        } while (Process32NextW(snapshot.get(), &pe32));

        return results;
    }
};

// ============================================================================
// 5. Configuration & Command Line Parser
// ============================================================================

// ============================================================================
// 5. Configuration & Command Line Parser
// ============================================================================
enum class OutputType { Table, CSV, JSON, Pipe };
enum class SortColumn { Threads, PID, Name };

struct AppConfig {
    OutputType format = OutputType::Table;
    SortColumn sortBy = SortColumn::Threads;
    bool sortDescending = true;
    size_t topN = 0;               // 0 = all
    DWORD minThreads = 0;          // Filter out processes below this count
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
                if (s == L"threads") config.sortBy = SortColumn::Threads;
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
            } else if ((arg == L"-m" || arg == L"--min-threads") && i + 1 < argc) {
                config.minThreads = std::stoul(argv[++i]);
            }
        }
        return config;
    }

    static void printHelp() {
        std::wcout << LR"(threads(1)              CrossShell for UNIX Reference Manual              threads(1)

    NAME
        threads - inspect and query thread counts across active processes

    SYNOPSIS
        threads [OPTIONS]
        threads [-f FORMAT] [-s COLUMN] [--asc|--desc] [-n NAME] [-p PID] [-m MIN] [-t COUNT]

    DESCRIPTION
        threads captures a snapshot of the active Windows process tree and
        enumerates thread allocations per process. It supports filtering by
        process identifier, process image name, or minimum thread threshold,
        sorting by thread count, and exporting structured data.

    OPTIONS
        -f, --format FORMAT
            Output serialization format: table, csv, json, or pipe.
            Default is table.

        -s, --sort COLUMN
            Sort process list by COLUMN: 'threads' (default), 'pid', or 'name'.

        --asc
            Sort records in ascending order.

        --desc
            Sort records in descending order (default).

        -n, --name STRING
            Filter processes matching the specified name substring.

        -p, --pid PID
            Filter by exact Process Identifier (PID).

        -m, --min-threads COUNT
            Filter out processes with fewer than COUNT threads.

        -t, --top N
            Limit output to the top N processes.

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
        threads
            Display thread counts for all active processes sorted descending.

        threads -t 10
            Display the top 10 processes with the highest thread counts.

        threads -n chrome --json
            Query thread counts for all 'chrome' processes formatted as JSON.

        threads -m 50 -f csv
            Export all processes with at least 50 threads in CSV format.

        threads -s name --asc
            List processes and their thread counts sorted alphabetically.

    CrossShell for UNIX                                                 threads(1)
)";
    }

    static void printVersion() {
        std::wcout << L"threads (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 PC/OpenSystems LLC contributors. All rights reserved.\n";
    }
};

// ============================================================================
// 6. Application Controller
// ============================================================================
class ThreadCountApp {
public:
    explicit ThreadCountApp(AppConfig config) : config_(std::move(config)) {}

    void run() {
        auto processes = ThreadCollector::collect();

        // 1. Filter by PID
        if (config_.filterPid != 0) {
            processes.erase(std::remove_if(processes.begin(), processes.end(),
                [this](const ProcessThreadInfo& item) {
                    return item.pid != config_.filterPid;
                }), processes.end());
        }

        // 2. Filter by Substring Name
        if (!config_.filterName.empty()) {
            std::wstring needle = toLower(config_.filterName);
            processes.erase(std::remove_if(processes.begin(), processes.end(),
                [&needle](const ProcessThreadInfo& item) {
                    return item.name.empty() || toLower(item.name).find(needle) == std::wstring::npos;
                }), processes.end());
        }

        // 3. Filter by Minimum Threads
        if (config_.minThreads > 0) {
            processes.erase(std::remove_if(processes.begin(), processes.end(),
                [this](const ProcessThreadInfo& item) {
                    return item.threadCount < config_.minThreads;
                }), processes.end());
        }

        // 4. Sort
        std::sort(processes.begin(), processes.end(), [this](const ProcessThreadInfo& a, const ProcessThreadInfo& b) {
            bool result = false;
            switch (config_.sortBy) {
                case SortColumn::Threads: result = a.threadCount < b.threadCount; break;
                case SortColumn::PID:     result = a.pid < b.pid; break;
                case SortColumn::Name:    result = a.name < b.name; break;
            }
            return config_.sortDescending ? !result : result;
        });

        // 5. Limit to Top N
        if (config_.topN > 0 && config_.topN < processes.size()) {
            processes.resize(config_.topN);
        }

        // 6. Format and Output
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

    ThreadCountApp app(config);
    app.run();
    return 0;
}