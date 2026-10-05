class FindmntReporter {
public:
    static void Report(const std::vector<MountEntry>& entries, const FindmntOptions& opts) {
        std::wstring output;
        size_t jsonIndex = 0;

        if (opts.outputFormat == OutputFormat::Default && opts.pipeCommand.empty() && !opts.noHeadings) {
            std::wcout << std::left
                       << std::setw(42) << L"SOURCE"
                       << std::setw(14) << L"TARGET"
                       << std::setw(14) << L"FSTYPE"
                       << L"OPTIONS\n";
        }

        for (const auto& entry : entries) {
            bool keep = opts.filters.empty();
            for (const auto& filter : opts.filters) {
                if (StringHelper::ContainsICase(entry.source, filter) ||
                    StringHelper::ContainsICase(entry.target, filter) ||
                    StringHelper::ContainsICase(entry.fsType, filter) ||
                    StringHelper::ContainsICase(entry.options, filter)) {
                    keep = true;
                    break;
                }
            }

            if (!keep) {
                continue;
            }

            if (opts.outputFormat == OutputFormat::Json) {
                if (jsonIndex++ != 0) output += L",\n";
                output += L"{\"source\":\"" + StringHelper::EscapeJson(entry.source) + 
                          L"\",\"target\":\"" + StringHelper::EscapeJson(entry.target) + 
                          L"\",\"fstype\":\"" + StringHelper::EscapeJson(entry.fsType) + 
                          L"\",\"options\":\"" + StringHelper::EscapeJson(entry.options) + L"\"}";
            } else if (opts.outputFormat == OutputFormat::Csv) {
                output += entry.source + L"," + entry.target + L"," + entry.fsType + L"," + entry.options + L"\n";
            } else if (opts.outputFormat == OutputFormat::Table || !opts.pipeCommand.empty()) {
                output += entry.source + L"\t" + entry.target + L"\t" + entry.fsType + L"\t" + entry.options + L"\n";
            } else {
                std::wcout << std::left
                           << std::setw(42) << entry.source
                           << std::setw(14) << entry.target
                           << std::setw(14) << entry.fsType
                           << entry.options << L"\n";
            }
        }

        if (opts.outputFormat != OutputFormat::Default || !opts.pipeCommand.empty()) {
            if (opts.outputFormat == OutputFormat::Json) output = L"[\n" + output + L"\n]\n";
            else if (opts.outputFormat == OutputFormat::Csv) output = L"source,target,fstype,options\n" + output;
            else if (opts.outputFormat == OutputFormat::Table) output = L"SOURCE\tTARGET\tFSTYPE\tOPTIONS\n" + output;

            if (!opts.pipeCommand.empty()) {
                ScopedWpOpen pipe(_wpopen(opts.pipeCommand.c_str(), L"w"));
                if (pipe.IsValid()) {
                    std::string narrow = StringHelper::ToUtf8(output);
                    fwrite(narrow.data(), 1, narrow.size(), pipe.Get());
                }
            } else {
                std::wcout << output;
            }
        }
    }
};
