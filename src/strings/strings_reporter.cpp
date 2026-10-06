#include "strings_reporter.hpp"

int StringsReporter::dispatch(const std::vector<std::string>& results, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "[\n";
            for (size_t i = 0; i < results.size(); ++i) {
                text += (i ? ",\n" : "") + std::string("  {\"string\":\"") + results[i] + "\"}";
            }
            text += "\n]\n";
        } else if (format == 2) {
            text = "string\n";
            for (const auto& s : results) {
                text += "\"" + s + "\"\n";
            }
        } else if (format == 3) {
            text = "STRINGS\n-------\n";
            for (const auto& s : results) {
                text += s + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else if (format != 0) {
            std::cout << text;
        }
        return 0;
    }
