#include "csv_formatter.hpp"
#include "json_formatter.hpp"
#include "output_formatter.hpp"
#include "pipe_formatter.hpp"
#include "process_thread_info.hpp"
#include "table_formatter.hpp"
#include "thread_collector.hpp"
#include "threads_app.hpp"
#include "threads_options.hpp"

ThreadCountApp::ThreadCountApp(AppConfig config) : config_(std::move(config)) {}

void ThreadCountApp::run() {
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

std::wstring ThreadCountApp::toLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
    }
