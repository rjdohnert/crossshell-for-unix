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

