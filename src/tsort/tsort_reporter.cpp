#include "tsort_reporter.hpp"

int TsortReporter::dispatch(const std::vector<std::string>& nodes, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "[\n";
            for (size_t i = 0; i < nodes.size(); ++i) {
                text += (i ? ",\n" : "") + std::string("  {\"node\":\"") + nodes[i] + "\"}";
            }
            text += "\n]\n";
        } else if (format == 2) {
            text = "node\n";
            for (const auto& n : nodes) {
                text += "\"" + n + "\"\n";
            }
        } else if (format == 3) {
            text = "NODE\n----\n";
            for (const auto& n : nodes) {
                text += n + "\n";
            }
        } else {
            for (const auto& n : nodes) {
                text += n + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
