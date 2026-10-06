#include "output_formatter.hpp"

std::string OutputFormatter::JsonQuote(const std::string& value) {
        std::string out = "\"";
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c < 0x20 ? '?' : static_cast<char>(c);
        }
        return out + "\"";
    }

std::string OutputFormatter::CsvQuote(const std::string& value) {
        std::string out = "\"";
        for (char c : value) out += c == '"' ? "\"\"" : std::string(1, c);
        return out + "\"";
    }

void OutputFormatter::EmitRecords(std::istream& input, OutputFormat format, FILE* pipe) {
        if (format == OutputFormat::Json && !pipe) std::cout << "[\n";
        bool first = true;
        auto emit = [&](const std::string& text) {
            if (pipe) std::fwrite(text.data(), 1, text.size(), pipe);
            else std::cout << text;
        };

        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (format == OutputFormat::Json) {
                if (!first) emit(",\n");
                first = false;
                emit("{\"match\":" + JsonQuote(line) + "}");
            } else if (format == OutputFormat::Csv) {
                emit(CsvQuote(line) + "\n");
            } else if (format == OutputFormat::Table) {
                emit(line + "\n");
            } else {
                emit(line + "\n");
            }
        }
        if (format == OutputFormat::Json) emit("\n]\n");
    }
