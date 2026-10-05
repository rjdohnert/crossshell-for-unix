#include "reporter.hpp"
#include <iomanip>
#include <iostream>

std::string LsattrReporter::ToUtf8(const std::wstring& value) {
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    if (n) {
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
    }
    return out;
}

std::string LsattrReporter::EscapeCsv(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        out += (c == '"') ? "\"\"" : std::string(1, c);
    }
    return out + '"';
}

std::string LsattrReporter::EscapeJson(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

void LsattrReporter::PrintRows(const std::vector<AttrRow>& rows, OutputFormat format) {
    if (format == OutputFormat::Csv) {
        std::cout << "flags,path\n";
        for (const auto& row : rows) {
            std::cout << EscapeCsv(ToUtf8(row.flags)) << ","
                      << EscapeCsv(ToUtf8(row.path)) << "\n";
        }
    } else if (format == OutputFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            std::cout << "  {\"flags\": \"" << EscapeJson(ToUtf8(rows[i].flags))
                      << "\", \"path\": \"" << EscapeJson(ToUtf8(rows[i].path)) << "\"}";
            if (i + 1 < rows.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "]\n";
    } else {
        std::wcout << L"Flags  Path\n------ ------------------------------------------------------------\n";
        for (const auto& row : rows) {
            std::wcout << std::left << std::setw(6) << row.flags << L" " << row.path << L"\n";
        }
    }
}
