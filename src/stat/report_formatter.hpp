#pragma once

#include "file_summary_report.hpp"
#include "stat.hpp"

class ReportFormatter {
public:
    static std::wstring formatSize(uint64_t bytes, bool human);

    static void renderDetailed(std::wostream& os, const FileSummaryReport& r, bool human);

    static void renderTable(std::wostream& os, const std::vector<FileSummaryReport>& reports, bool human);

    static void renderJSON(std::wostream& os, const std::vector<FileSummaryReport>& reports);

    static void renderCSV(std::wostream& os, const std::vector<FileSummaryReport>& reports);

private:
    static void printCell(std::wostream& os, const std::wstring& text, size_t width, bool rightAlign, bool isLast);

    static std::wstring escapeJSON(const std::wstring& s);
};
