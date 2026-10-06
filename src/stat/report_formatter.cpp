#include "file_summary_report.hpp"
#include "report_formatter.hpp"

std::wstring ReportFormatter::formatSize(uint64_t bytes, bool human) {
        if (!human) return std::to_wstring(bytes) + L" B";
        const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int idx = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && idx < 4) {
            size /= 1024.0;
            idx++;
        }
        std::wostringstream oss;
        if (idx == 0) oss << bytes << L" B";
        else oss << std::fixed << std::setprecision(1) << size << L" " << units[idx];
        return oss.str();
    }

void ReportFormatter::renderDetailed(std::wostream& os, const FileSummaryReport& r, bool human) {
        os << L"\n";
        os << L" File Summary: " << r.fileName << L"\n";
        os << L"\n";
        os << L"  Full Path      : " << r.filePath << L"\n";
        os << L"  Classification : " << r.fileType << L"\n";
        os << L"  Owner          : " << r.owner << L"\n";
        os << L"  Created        : " << r.creationTime << L"\n";
        os << L"  Last Modified  : " << r.lastModifiedTime << L"\n";
        os << L"  File Size      : " << formatSize(r.byteSize, human) << L" (" << r.byteSize << L" bytes)\n";
        os << L"  Line Count     : ";
        if (r.lineCount >= 0) {
            os << r.lineCount << L" lines\n";
        } else {
            os << L"N/A (Binary or Directory)\n";
        }
        os << L"\n";
    }

void ReportFormatter::renderTable(std::wostream& os, const std::vector<FileSummaryReport>& reports, bool human) {
        std::vector<std::wstring> headers = { L"File Name", L"Type", L"Lines", L"Owner", L"Created", L"Size" };
        std::vector<std::vector<std::wstring>> rows;

        for (const auto& r : reports) {
            if (!r.isAccessible) {
                rows.push_back({ r.filePath, L"ERROR: " + r.errorMessage, L"-", L"-", L"-", L"-" });
                continue;
            }
            std::wstring lineStr = (r.lineCount >= 0) ? std::to_wstring(r.lineCount) : L"-";
            rows.push_back({
                r.fileName,
                r.fileType,
                lineStr,
                r.owner,
                r.creationTime,
                formatSize(r.byteSize, human)
            });
        }

        std::vector<size_t> widths(headers.size(), 0);
        for (size_t i = 0; i < headers.size(); ++i) widths[i] = headers[i].length();
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                widths[i] = std::max(widths[i], row[i].length());
            }
        }

        // Print Header
        for (size_t i = 0; i < headers.size(); ++i) {
            bool rightAlign = (i == 2 || i == 5); // Lines and Size
            printCell(os, headers[i], widths[i], rightAlign, i == headers.size() - 1);
        }
        os << L"\n";

        // Print Separator
        for (size_t i = 0; i < headers.size(); ++i) {
            os << std::wstring(widths[i], L'-');
            if (i < headers.size() - 1) os << L"  ";
        }
        os << L"\n";

        // Print Rows
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                bool rightAlign = (i == 2 || i == 5);
                printCell(os, row[i], widths[i], rightAlign, i == row.size() - 1);
            }
            os << L"\n";
        }
    }

void ReportFormatter::renderJSON(std::wostream& os, const std::vector<FileSummaryReport>& reports) {
        os << L"[\n";
        for (size_t i = 0; i < reports.size(); ++i) {
            const auto& r = reports[i];
            os << L"  {\n";
            os << L"    \"path\": \"" << escapeJSON(r.filePath) << L"\",\n";
            os << L"    \"name\": \"" << escapeJSON(r.fileName) << L"\",\n";
            os << L"    \"type\": \"" << escapeJSON(r.fileType) << L"\",\n";
            os << L"    \"owner\": \"" << escapeJSON(r.owner) << L"\",\n";
            os << L"    \"created\": \"" << escapeJSON(r.creationTime) << L"\",\n";
            os << L"    \"size_bytes\": " << r.byteSize << L",\n";
            os << L"    \"lines\": " << r.lineCount << L",\n";
            os << L"    \"is_text\": " << (r.isText ? L"true" : L"false") << L"\n";
            os << L"  }" << (i + 1 < reports.size() ? L"," : L"") << L"\n";
        }
        os << L"]\n";
    }

void ReportFormatter::renderCSV(std::wostream& os, const std::vector<FileSummaryReport>& reports) {
        os << L"Path,Name,Type,Owner,Created,SizeBytes,Lines,IsText\n";
        for (const auto& r : reports) {
            os << L"\"" << r.filePath << L"\",\""
               << r.fileName << L"\",\""
               << r.fileType << L"\",\""
               << r.owner << L"\",\""
               << r.creationTime << L"\","
               << r.byteSize << L","
               << r.lineCount << L","
               << (r.isText ? L"true" : L"false") << L"\n";
        }
    }

void ReportFormatter::printCell(std::wostream& os, const std::wstring& text, size_t width, bool rightAlign, bool isLast) {
        if (rightAlign) {
            os << std::setw(static_cast<int>(width)) << text;
        } else {
            os << std::left << std::setw(static_cast<int>(width)) << text << std::right;
        }
        if (!isLast) os << L"  ";
    }

std::wstring ReportFormatter::escapeJSON(const std::wstring& s) {
        std::wstring res;
        for (wchar_t c : s) {
            if (c == L'\\') res += L"\\\\";
            else if (c == L'\"') res += L"\\\"";
            else if (c == L'\n') res += L"\\n";
            else if (c == L'\r') res += L"\\r";
            else if (c == L'\t') res += L"\\t";
            else res += c;
        }
        return res;
    }
