#include "stop_reporter.hpp"

void StopReporter::Report(OutputFormat format, const std::string& pipeCommand, size_t count, bool hasErrors) {
        if (format == OutputFormat::None && pipeCommand.empty()) {
            return;
        }

        std::string status = hasErrors ? "failure" : "success";
        std::string text;

        if (format == OutputFormat::Json) {
            text = "{\"status\":\"" + status + "\",\"count\":" + std::to_string(count) + "}\n";
        } else if (format == OutputFormat::Csv) {
            text = "status,count\n" + status + "," + std::to_string(count) + "\n";
        } else if (format == OutputFormat::Table) {
            text = "STATUS\tCOUNT\n" + status + "\t" + std::to_string(count) + "\n";
        } else {
            text = status + ": " + std::to_string(count) + "\n";
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return;
            fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
    }
