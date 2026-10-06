#pragma once

#include "file_summary_report.hpp"
#include "stat.hpp"

class FileAnalyzer {
public:
    static FileSummaryReport analyzeFile(const std::wstring& rawPath);

private:
    static std::wstring formatFileTime(const FILETIME& ft);
};
