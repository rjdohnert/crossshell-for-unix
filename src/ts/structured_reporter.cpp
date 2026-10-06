#include "structured_reporter.hpp"

std::string StructuredReporter::jsonQuote(const std::string& value) {
        std::string out = "\"";
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += (c < 0x20) ? '?' : static_cast<char>(c);
        }
        return out + "\"";
    }

std::string StructuredReporter::csvQuote(const std::string& value) {
        std::string out = "\"";
        for (char c : value) {
            out += (c == '"') ? "\"\"" : std::string(1, c);
        }
        return out + "\"";
    }

int StructuredReporter::output(const std::vector<std::pair<std::string, std::string>>& records,
                      int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"events\":[";
            for (size_t i = 0; i < records.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"timestamp\":" + jsonQuote(records[i].first) + ",\"line\":" + jsonQuote(records[i].second) + "}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "\"timestamp\",\"line\"\n";
            for (const auto& r : records) {
                text += csvQuote(r.first) + "," + csvQuote(r.second) + "\n";
            }
        } else if (format == 3) {
            text = "TIMESTAMP\tLINE\n--------------------\n";
            for (const auto& r : records) {
                text += r.first + "\t" + r.second + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::fwrite(text.data(), 1, text.size(), stdout);
        }
        return 0;
    }
