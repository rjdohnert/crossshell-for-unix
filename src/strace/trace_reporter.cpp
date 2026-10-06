#include "trace_formatter.hpp"
#include "trace_options.hpp"
#include "trace_reporter.hpp"

std::wstring TraceReporter::FormatIpPrefix(const TraceOptions& options, HANDLE hThread, DWORD64 defaultIp) {
    if (!options.printIp) return L"";
    DWORD64 ip = defaultIp;
    if (ip == 0 && hThread != NULL) {
        CONTEXT context = {};
        context.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(hThread, &context)) {
#ifdef _M_X64
            ip = context.Rip;
#else
            ip = context.Eip;
#endif
        }
    }
    return L"[" + TraceFormatter::ToHex(ip, 16) + L"] ";
}

void TraceReporter::EmitTraceLine(TraceOptions& options, const std::wstring& line, HANDLE hThread, DWORD64 defaultIp, bool isEvent) {
    if (isEvent && options.summaryOnly) return;
    if (!isEvent && options.outputFormat != OutputFormat::Human) return;

    std::wstring prefix = L"";
    if (options.timestamp) {
        prefix += L"[" + TraceFormatter::FormatTimestamp() + L"] ";
    }
    if (isEvent && options.printIp) {
        prefix += FormatIpPrefix(options, hThread, defaultIp);
    }
    if (options.outputFormat == OutputFormat::Json) {
        if (!options.jsonFirstRecord) std::wcout << L",\n";
        options.jsonFirstRecord = false;
        std::wcout << L"{\"type\":\"event\",\"message\":\"" << TraceFormatter::JsonEscape(prefix + line) << L"\"}" << std::endl;
    } else if (options.outputFormat == OutputFormat::Csv) {
        std::wcout << TraceFormatter::CsvEscape(L"event") << L"," << TraceFormatter::CsvEscape(prefix + line) << std::endl;
    } else if (options.outputFormat == OutputFormat::Table) {
        static bool headerWritten = false;
        if (!headerWritten) { std::wcout << L"TYPE\tMESSAGE\n"; headerWritten = true; }
        std::wcout << L"event\t" << prefix << line << std::endl;
    } else {
        std::wcout << prefix << line << std::endl;
    }
}

bool TraceReporter::ShouldPrintEvent(const TraceOptions& options, const std::wstring& category) {
    if (!options.filterEvents) return true;
    for (const auto& filter : options.eventFilters) {
        if (filter == category) return true;
    }
    return false;
}

void TraceReporter::PrintSummary(const std::unordered_map<std::wstring, SummaryStats>& summaryData) {
    double totalSeconds = 0.0;
    DWORD totalCalls = 0;
    for (const auto& entry : summaryData) {
        totalSeconds += entry.second.seconds;
        totalCalls += entry.second.calls;
    }

    struct SummaryEntry {
        std::wstring name;
        SummaryStats stats;
    };
    std::vector<SummaryEntry> sortedEntries;
    for (const auto& entry : summaryData) {
        sortedEntries.push_back({ entry.first, entry.second });
    }
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const SummaryEntry& a, const SummaryEntry& b) {
        return a.stats.seconds > b.stats.seconds;
    });

    std::wcout << std::endl;
    std::wcout << std::left 
               << std::setw(10) << L"% time"
               << std::setw(12) << L"seconds"
               << std::setw(12) << L"usecs/call"
               << std::setw(10) << L"calls"
               << L"event" << std::endl;
    std::wcout << std::wstring(64, L'-') << std::endl;

    for (const auto& entry : sortedEntries) {
        double pct = (totalSeconds > 0.0) ? (entry.stats.seconds / totalSeconds * 100.0) : 0.0;
        double usecsPerCall = (entry.stats.calls > 0) ? (entry.stats.seconds / entry.stats.calls * 1000000.0) : 0.0;

        std::wcout << std::left << std::fixed << std::setprecision(2)
                   << std::setw(10) << pct
                   << std::setw(12) << entry.stats.seconds
                   << std::setw(12) << static_cast<DWORD>(usecsPerCall)
                   << std::setw(10) << entry.stats.calls
                   << entry.name << std::endl;
    }
    std::wcout << std::wstring(64, L'-') << std::endl;
    std::wcout << std::left
               << std::setw(10) << 100.00
               << std::setw(12) << totalSeconds
               << std::setw(12) << L""
               << std::setw(10) << totalCalls
               << L"total" << std::endl;
}
