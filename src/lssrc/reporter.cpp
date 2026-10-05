#include "reporter.hpp"
#include <iostream>
#include <iomanip>

std::string LssrcReporter::CsvEscape(const std::string& value) {
    std::string out = "\"";
    for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
    return out + '"';
}

std::string LssrcReporter::JsonEscape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

void LssrcReporter::Emit(const LssrcOptions& opts, const std::vector<ServiceRow>& rows) {
    if (opts.format == LssrcFormat::Csv) {
        std::cout << "Service,PID,Status\n";
        for (const auto& row : rows) {
            std::cout << CsvEscape(row.name) << ',' << row.pid << ',' << CsvEscape(row.status) << '\n';
        }
        return;
    }

    if (opts.format == LssrcFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            std::cout << "  {\"service\":\"" << JsonEscape(row.name) << "\",\"pid\":" << row.pid
                      << ",\"status\":\"" << JsonEscape(row.status) << "\"}"
                      << (i + 1 == rows.size() ? "\n" : ",\n");
        }
        std::cout << "]\n";
        return;
    }

    std::cout << std::left << std::setw(36) << "Subsystem" << std::setw(8) << "PID" << "Status\n";
    for (const auto& row : rows) {
        std::cout << std::setw(36) << row.name << std::setw(8) << row.pid << row.status << '\n';
    }
}
