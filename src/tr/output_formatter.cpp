#include "output_formatter.hpp"

std::string OutputFormatter::jsonQuote(const std::string& value) {
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

std::string OutputFormatter::csvQuote(const std::string& value) {
        std::string out = "\"";
        for (char c : value) {
            out += (c == '"') ? "\"\"" : std::string(1, c);
        }
        return out + "\"";
    }

int OutputFormatter::output(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"output\":" + jsonQuote(content) + "}\n";
        } else if (format == 2) {
            text = "\"output\"\n" + csvQuote(content) + "\n";
        } else if (format == 3) {
            text = "OUTPUT\n------\n" + content;
        } else {
            text = content;
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
