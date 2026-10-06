#include "file_analyzer.hpp"
#include "file_classifier.hpp"
#include "file_summary_report.hpp"
#include "security_inspector.hpp"

FileSummaryReport FileAnalyzer::analyzeFile(const std::wstring& rawPath) {
        FileSummaryReport report;
        report.filePath = rawPath;

        WIN32_FILE_ATTRIBUTE_DATA attrData;
        if (!GetFileAttributesExW(rawPath.c_str(), GetFileExInfoStandard, &attrData)) {
            report.isAccessible = false;
            report.errorMessage = L"File not found or inaccessible";
            return report;
        }

        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            report.isAccessible = true;
            report.fileName = fs::path(rawPath).filename().wstring();
            report.fileType = L"Directory / Folder";
            report.owner = SecurityInspector::getFileOwner(rawPath);
            report.creationTime = formatFileTime(attrData.ftCreationTime);
            report.lastModifiedTime = formatFileTime(attrData.ftLastWriteTime);
            report.lineCount = -1;
            report.byteSize = 0;
            return report;
        }

        report.isAccessible = true;
        report.fileName = fs::path(rawPath).filename().wstring();
        report.byteSize = (static_cast<uint64_t>(attrData.nFileSizeHigh) << 32) | attrData.nFileSizeLow;
        report.creationTime = formatFileTime(attrData.ftCreationTime);
        report.lastModifiedTime = formatFileTime(attrData.ftLastWriteTime);
        report.owner = SecurityInspector::getFileOwner(rawPath);

        auto analysis = FileClassifier::analyze(rawPath, report.byteSize);
        report.fileType = analysis.typeDescription;
        report.isText = analysis.isText;
        report.lineCount = analysis.lineCount;

        return report;
    }

std::wstring FileAnalyzer::formatFileTime(const FILETIME& ft) {
        FILETIME localFt;
        FileTimeToLocalFileTime(&ft, &localFt);
        SYSTEMTIME st;
        FileTimeToSystemTime(&localFt, &st);

        WCHAR dateBuf[64] = { 0 };
        WCHAR timeBuf[64] = { 0 };
        GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, dateBuf, 64);
        GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, nullptr, timeBuf, 64);

        return std::wstring(dateBuf) + L" " + timeBuf;
    }
