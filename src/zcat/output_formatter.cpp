#include "output_formatter.hpp"

std::string OutputFormatter::QuoteJson(const std::string& value) {
        std::string out = "\"";
        for (unsigned char ch : value) {
            if (ch == '"' || ch == '\\') out += '\\';
            if (ch == '\n') out += "\\n";
            else if (ch == '\r') out += "\\r";
            else if (ch == '\t') out += "\\t";
            else if (ch < 0x20) out += '?';
            else out += static_cast<char>(ch);
        }
        return out + "\"";
    }

std::string OutputFormatter::QuoteCsv(const std::string& value) {
        std::string out = "\"";
        for (char ch : value) out += (ch == '"') ? "\"\"" : std::string(1, ch);
        return out + "\"";
    }

void OutputFormatter::EmitData(const std::string& data, OutputFormat format, std::ostream& out) {
        if (format == OutputFormat::Json) {
            out << "{\"content\":" << QuoteJson(data) << "}\n";
        } else if (format == OutputFormat::Csv) {
            out << "\"content\"\n" << QuoteCsv(data) << "\n";
        } else if (format == OutputFormat::Table) {
            out << "CONTENT\n-------\n" << data << (data.empty() || data.back() == '\n' ? "" : "\n");
        }
    }
