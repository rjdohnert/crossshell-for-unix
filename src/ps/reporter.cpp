#include "reporter.hpp"

// TableFormatter
void TableFormatter::render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) {
    if (columns.empty()) return;

    std::map<std::wstring, size_t> widths;
    for (const auto& col : columns) {
        widths[col] = col.length();
    }

    for (const auto& r : records) {
        for (const auto& col : columns) {
            widths[col] = (std::max)(widths[col], r.getField(col).length());
        }
    }

    // Header
    for (size_t i = 0; i < columns.size(); ++i) {
        std::wstring colUpper = columns[i];
        std::transform(colUpper.begin(), colUpper.end(), colUpper.begin(), ::towupper);
        std::wcout << std::left << std::setw(widths[columns[i]] + 2) << colUpper;
    }
    std::wcout << L"\n";

    // Underline Separator
    for (size_t i = 0; i < columns.size(); ++i) {
        std::wcout << std::wstring(widths[columns[i]], L'-') << L"  ";
    }
    std::wcout << L"\n";

    // Rows
    for (const auto& r : records) {
        for (size_t i = 0; i < columns.size(); ++i) {
            std::wcout << std::left << std::setw(widths[columns[i]] + 2) << r.getField(columns[i]);
        }
        std::wcout << L"\n";
    }
}

// CsvFormatter
void CsvFormatter::render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) {
    for (size_t i = 0; i < columns.size(); ++i) {
        std::wcout << L"\"" << columns[i] << L"\"" << (i + 1 < columns.size() ? L"," : L"\n");
    }
    for (const auto& r : records) {
        for (size_t i = 0; i < columns.size(); ++i) {
            std::wstring val = r.getField(columns[i]);
            size_t pos = 0;
            while ((pos = val.find(L"\"", pos)) != std::wstring::npos) {
                val.replace(pos, 1, L"\"\"");
                pos += 2;
            }
            std::wcout << L"\"" << val << L"\"" << (i + 1 < columns.size() ? L"," : L"\n");
        }
    }
}

// JsonFormatter
std::wstring JsonFormatter::escapeJson(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c == L'\"') out += L"\\\"";
        else if (c == L'\\') out += L"\\\\";
        else if (c == L'\b') out += L"\\b";
        else if (c == L'\f') out += L"\\f";
        else if (c == L'\n') out += L"\\n";
        else if (c == L'\r') out += L"\\r";
        else if (c == L'\t') out += L"\\t";
        else out += c;
    }
    return out;
}

void JsonFormatter::render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) {
    std::wcout << L"[\n";
    for (size_t i = 0; i < records.size(); ++i) {
        std::wcout << L"  {\n";
        for (size_t j = 0; j < columns.size(); ++j) {
            std::wstring val = records[i].getField(columns[j]);
            std::wcout << L"    \"" << columns[j] << L"\": \"" << escapeJson(val) << L"\""
                       << (j + 1 < columns.size() ? L",\n" : L"\n");
        }
        std::wcout << L"  }" << (i + 1 < records.size() ? L",\n" : L"\n");
    }
    std::wcout << L"]\n";
}

// XmlFormatter
std::wstring XmlFormatter::escapeXml(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        if (c == L'&') out += L"&amp;";
        else if (c == L'<') out += L"&lt;";
        else if (c == L'>') out += L"&gt;";
        else if (c == L'\"') out += L"&quot;";
        else if (c == L'\'') out += L"&apos;";
        else out += c;
    }
    return out;
}

void XmlFormatter::render(const std::vector<ProcessRecord>& records, const std::vector<std::wstring>& columns) {
    std::wcout << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    std::wcout << L"<processes count=\"" << records.size() << L"\">\n";
    for (const auto& r : records) {
        std::wcout << L"  <process>\n";
        for (const auto& col : columns) {
            std::wcout << L"    <" << col << L">"
                       << escapeXml(r.getField(col))
                       << L"</" << col << L">\n";
        }
        std::wcout << L"  </process>\n";
    }
    std::wcout << L"</processes>\n";
}
