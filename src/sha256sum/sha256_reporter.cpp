#include "sha256_reporter.hpp"

int Sha256Reporter::dispatch(const std::vector<Sha256Result>& results, int format, const std::string& pipeCommand) {
    std::string text;
    if (format == 1) {
        text = "{\"sha256\":[";
        for (size_t i = 0; i < results.size(); ++i) {
            if (i > 0) text += ",";
            text += "{\"file\":\"" + results[i].filename + "\",\"hash\":\"" + results[i].hash + "\"}";
        }
        text += "]}\n";
    } else if (format == 2) {
        text = "hash,file\n";
        for (const auto& r : results) {
            text += "\"" + r.hash + "\",\"" + r.filename + "\"\n";
        }
    } else if (format == 3) {
        text = "HASH                                                              FILE\n----------------------------------------------------------------  ----\n";
        for (const auto& r : results) {
            text += r.hash + "  " + r.filename + "\n";
        }
    } else {
        for (const auto& r : results) {
            if (!r.error) {
                text += r.hash + " " + (r.isBinary ? "*" : " ") + r.filename + "\n";
            }
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
