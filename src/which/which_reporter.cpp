#include "which_reporter.hpp"

int WhichReporter::dispatch(const std::vector<std::pair<std::string, std::string>>& matches,
                        int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"matches\":[";
            for (size_t i = 0; i < matches.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"command\":\"" + matches[i].first + "\",\"path\":\"" + matches[i].second + "\"}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "command,path\n";
            for (const auto& m : matches) {
                text += "\"" + m.first + "\",\"" + m.second + "\"\n";
            }
        } else if (format == 3) {
            text = "COMMAND\tPATH\n--------------------\n";
            for (const auto& m : matches) {
                text += m.first + "\t" + m.second + "\n";
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
