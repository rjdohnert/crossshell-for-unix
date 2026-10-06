#include "telemetry_reporter.hpp"
#include "duration_parser.hpp"

void TelemetryReporter::report(const SleepOptions& options, long long msToSleep, double totalSleepMs) {
    if (options.format == OutputFormat::None && options.pipe_command.empty()) {
        return;
    }

    std::wstring text;
    if (options.format == OutputFormat::Json) {
        text = L"{\"status\":\"completed\",\"milliseconds\":" + std::to_wstring(msToSleep) +
               L",\"seconds\":" + std::to_wstring(totalSleepMs / 1000.0) + L"}\n";
    } else if (options.format == OutputFormat::Csv) {
        text = L"status,milliseconds,seconds\ncompleted," + std::to_wstring(msToSleep) +
               L"," + std::to_wstring(totalSleepMs / 1000.0) + L"\n";
    } else if (options.format == OutputFormat::Tsv) {
        text = L"status\tmilliseconds\tseconds\ncompleted\t" + std::to_wstring(msToSleep) +
               L"\t" + std::to_wstring(totalSleepMs / 1000.0) + L"\n";
    } else {
        text = L"STATUS\tMILLISECONDS\tSECONDS\ncompleted\t" + std::to_wstring(msToSleep) +
               L"\t" + std::to_wstring(totalSleepMs / 1000.0) + L"\n";
    }

    if (!options.pipe_command.empty()) {
        FILE* pipe = _wpopen(options.pipe_command.c_str(), L"w");
        if (pipe) {
            std::string narrow = DurationParser::wideToUtf8(text);
            std::fwrite(narrow.data(), 1, narrow.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
    } else {
        std::wcout << text;
    }
}
