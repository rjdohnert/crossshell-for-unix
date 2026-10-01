/*
BSD 3-Clause License

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "Advapi32.lib")

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;

// ============================================================================
// Data Models & Options
// ============================================================================

enum class OutputFormat {
    Auto,       // Detailed card if 1 file, Table if multiple
    Table,      // Force tabular format
    Detailed,   // Force detailed card format
    JSON,       // JSON output for scripting pipelines
    CSV         // CSV format for pipeline processing
};

struct ProgramOptions {
    OutputFormat format = OutputFormat::Auto;
    bool humanReadable = true;
    bool showHelp = false;
    bool showVersion = false;
    std::vector<std::wstring> targetPaths;
};

struct FileSummaryReport {
    std::wstring filePath;
    std::wstring fileName;
    std::wstring fileType;
    std::wstring owner;
    std::wstring creationTime;
    std::wstring lastModifiedTime;
    uint64_t byteSize = 0;
    int64_t lineCount = -1; // -1 if binary or not applicable
    bool isText = false;
    bool isAccessible = false;
    std::wstring errorMessage;
};

// ============================================================================
// Windows NT Security Inspector (File Owner Resolution)
// ============================================================================

class SecurityInspector {
public:
    static std::wstring getFileOwner(const std::wstring& path) {
        DWORD lengthNeeded = 0;
        // Query required buffer size for the security descriptor
        GetFileSecurityW(path.c_str(), OWNER_SECURITY_INFORMATION, nullptr, 0, &lengthNeeded);
        if (lengthNeeded == 0) {
            return L"UNKNOWN";
        }

        std::vector<BYTE> sdBuffer(lengthNeeded);
        auto pSD = reinterpret_cast<PSECURITY_DESCRIPTOR>(sdBuffer.data());

        if (!GetFileSecurityW(path.c_str(), OWNER_SECURITY_INFORMATION, pSD, lengthNeeded, &lengthNeeded)) {
            return L"UNKNOWN";
        }

        PSID pOwnerSid = nullptr;
        BOOL bOwnerDefaulted = FALSE;
        if (!GetSecurityDescriptorOwner(pSD, &pOwnerSid, &bOwnerDefaulted) || !pOwnerSid) {
            return L"UNKNOWN";
        }

        WCHAR nameBuffer[256] = { 0 };
        DWORD nameLen = 256;
        WCHAR domainBuffer[256] = { 0 };
        DWORD domainLen = 256;
        SID_NAME_USE sidType;

        if (LookupAccountSidW(nullptr, pOwnerSid, nameBuffer, &nameLen, domainBuffer, &domainLen, &sidType)) {
            if (domainLen > 0) {
                return std::wstring(domainBuffer) + L"\\" + nameBuffer;
            }
            return std::wstring(nameBuffer);
        }

        return L"UNKNOWN";
    }
};

// ============================================================================
// File Content Classifier & Line Counter
// ============================================================================

class FileClassifier {
public:
    struct AnalysisResult {
        std::wstring typeDescription;
        bool isText = false;
        int64_t lineCount = -1;
    };

    static AnalysisResult analyze(const std::wstring& path, uint64_t fileSize) {
        AnalysisResult result;

        if (fileSize == 0) {
            result.typeDescription = L"Empty File";
            result.isText = true;
            result.lineCount = 0;
            return result;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            result.typeDescription = L"Unreadable File";
            return result;
        }

        // Read magic header bytes
        const size_t sampleSize = 4096;
        std::vector<char> buffer(sampleSize);
        file.read(buffer.data(), sampleSize);
        std::streamsize bytesRead = file.gcount();

        // 1. Identify Magic Numbers / Known Signatures
        std::wstring magicType = identifyMagicBytes(reinterpret_cast<const uint8_t*>(buffer.data()), bytesRead);
        if (!magicType.empty()) {
            result.typeDescription = magicType;
            result.isText = false;
            result.lineCount = -1;
            return result;
        }

        // 2. Binary vs Text Heuristic (Check for NULL bytes or high control codes)
        bool isBinary = false;
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            uint8_t ch = static_cast<uint8_t>(buffer[i]);
            if (ch == 0 || (ch < 7 || (ch > 14 && ch < 32 && ch != 27))) {
                isBinary = true;
                break;
            }
        }

        if (isBinary) {
            result.typeDescription = L"Binary Data";
            result.isText = false;
            result.lineCount = -1;
            return result;
        }

        // 3. Text file categorization by extension
        fs::path p(path);
        std::wstring ext = p.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        result.typeDescription = classifyTextExtension(ext);
        result.isText = true;

        // 4. Count lines across the entire file
        file.clear();
        file.seekg(0, std::ios::beg);

        int64_t lines = 0;
        char chunk[65536];
        bool hasChars = false;
        char lastChar = '\0';

        while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0) {
            std::streamsize n = file.gcount();
            hasChars = true;
            for (std::streamsize i = 0; i < n; ++i) {
                if (chunk[i] == '\n') {
                    lines++;
                }
                lastChar = chunk[i];
            }
        }

        // If file has content and does not end with \n, count trailing line
        if (hasChars && lastChar != '\n') {
            lines++;
        }

        result.lineCount = lines;
        return result;
    }

private:
    static std::wstring identifyMagicBytes(const uint8_t* buf, std::streamsize len) {
        if (len >= 2 && buf[0] == 'M' && buf[1] == 'Z') return L"Executable (Win32 PE/DLL)";
        if (len >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') return L"ELF Binary";
        if (len >= 4 && buf[0] == 'P' && buf[1] == 'K' && buf[2] == 0x03 && buf[3] == 0x04) return L"ZIP Archive / Office OpenXML";
        if (len >= 4 && buf[0] == '%' && buf[1] == 'P' && buf[2] == 'D' && buf[3] == 'F') return L"PDF Document";
        if (len >= 8 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') return L"PNG Image";
        if (len >= 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) return L"JPEG Image";
        if (len >= 6 && (memcmp(buf, "GIF87a", 6) == 0 || memcmp(buf, "GIF89a", 6) == 0)) return L"GIF Image";
        if (len >= 6 && memcmp(buf, "7z\xBC\xAF\x27\x1C", 6) == 0) return L"7-Zip Archive";
        if (len >= 7 && memcmp(buf, "Rar!\x1A\x07\x00", 7) == 0) return L"RAR Archive";
        if (len >= 2 && buf[0] == 0x1F && buf[1] == 0x8B) return L"GZIP Compressed File";
        if (len >= 4 && (memcmp(buf, "RIFF", 4) == 0)) return L"RIFF Container (WAV/AVI/WEBP)";
        return L"";
    }

    static std::wstring classifyTextExtension(const std::wstring& ext) {
        if (ext == L".cpp" || ext == L".cxx" || ext == L".cc" || ext == L".h" || ext == L".hpp") return L"C/C++ Source";
        if (ext == L".cs") return L"C# Source";
        if (ext == L".rs") return L"Rust Source";
        if (ext == L".py") return L"Python Script";
        if (ext == L".js" || ext == L".mjs" || ext == L".ts") return L"JavaScript/TypeScript";
        if (ext == L".json") return L"JSON Document";
        if (ext == L".xml" || ext == L".xaml") return L"XML Document";
        if (ext == L".html" || ext == L".htm") return L"HTML Document";
        if (ext == L".css" || ext == L".scss") return L"Cascading Style Sheet";
        if (ext == L".md" || ext == L".markdown") return L"Markdown Document";
        if (ext == L".txt" || ext == L".log") return L"Plain Text Document";
        if (ext == L".ps1" || ext == L".bat" || ext == L".cmd") return L"Windows Script/Batch";
        if (ext == L".sh" || ext == L".bash") return L"Shell Script";
        if (ext == L".sql") return L"SQL Database Script";
        if (ext == L".csv") return L"CSV Data File";
        if (ext == L".yaml" || ext == L".yml") return L"YAML Document";
        return L"Text Document";
    }
};

// ============================================================================
// File Analyzer Service
// ============================================================================

class FileAnalyzer {
public:
    static FileSummaryReport analyzeFile(const std::wstring& rawPath) {
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

private:
    static std::wstring formatFileTime(const FILETIME& ft) {
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
};

// ============================================================================
// Output Formatters (Table, Detailed Card, CSV, JSON)
// ============================================================================

class ReportFormatter {
public:
    static std::wstring formatSize(uint64_t bytes, bool human) {
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

    static void renderDetailed(std::wostream& os, const FileSummaryReport& r, bool human) {
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

    static void renderTable(std::wostream& os, const std::vector<FileSummaryReport>& reports, bool human) {
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

    static void renderJSON(std::wostream& os, const std::vector<FileSummaryReport>& reports) {
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

    static void renderCSV(std::wostream& os, const std::vector<FileSummaryReport>& reports) {
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

private:
    static void printCell(std::wostream& os, const std::wstring& text, size_t width, bool rightAlign, bool isLast) {
        if (rightAlign) {
            os << std::setw(static_cast<int>(width)) << text;
        } else {
            os << std::left << std::setw(static_cast<int>(width)) << text << std::right;
        }
        if (!isLast) os << L"  ";
    }

    static std::wstring escapeJSON(const std::wstring& s) {
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
};

// ============================================================================
// Pipeline Manager & Argument Parser
// ============================================================================

class CommandLineParser {
public:
    static ProgramOptions parse(int argc, wchar_t* argv[]) {
        ProgramOptions opts;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-?" || arg == L"/?" || arg == L"-h" || arg == L"--help") {
                opts.showHelp = true;
            } else if (arg == L"-v" || arg == L"-V" || arg == L"--version") {
                opts.showVersion = true;
            } else if (arg == L"-t" || arg == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == L"-d" || arg == L"--detailed") {
                opts.format = OutputFormat::Detailed;
            } else if (arg == L"--json") {
                opts.format = OutputFormat::JSON;
            } else if (arg == L"--csv") {
                opts.format = OutputFormat::CSV;
            } else if (arg == L"-b" || arg == L"--bytes") {
                opts.humanReadable = false;
            } else if (arg.rfind(L"-", 0) != 0) {
                opts.targetPaths.push_back(arg);
            }
        }
        return opts;
    }
};

class PipelineManager {
public:
    static bool isInputPiped() {
        return _isatty(_fileno(stdin)) == 0;
    }

    static std::vector<std::wstring> readPipedPaths() {
        std::vector<std::wstring> paths;
        std::wstring line;
        while (std::getline(std::wcin, line)) {
            // Trim whitespace / quotes
            line.erase(0, line.find_first_not_of(L" \t\r\n\""));
            line.erase(line.find_last_not_of(L" \t\r\n\"") + 1);
            if (!line.empty()) {
                paths.push_back(line);
            }
        }
        return paths;
    }
};

// ============================================================================
// Core Application Controller
// ============================================================================

class FileSummaryApplication {
public:
    explicit FileSummaryApplication(ProgramOptions options)
        : m_opts(std::move(options)) {}

    int run() {
        if (m_opts.showHelp) {
            printHelp();
            return 0;
        }

        if (m_opts.showVersion) {
            printVersion();
            return 0;
        }

        // Support for piping file paths through stdin
        if (m_opts.targetPaths.empty() && PipelineManager::isInputPiped()) {
            auto piped = PipelineManager::readPipedPaths();
            m_opts.targetPaths.insert(m_opts.targetPaths.end(), piped.begin(), piped.end());
        }

        if (m_opts.targetPaths.empty()) {
            std::wcerr << L"stat: error: no files specified.\n";
            std::wcerr << L"Try 'stat --help' or 'stat -?' for more information.\n";
            return 1;
        }

        std::vector<FileSummaryReport> reports;
        for (const auto& path : m_opts.targetPaths) {
            reports.push_back(FileAnalyzer::analyzeFile(path));
        }

        render(reports);
        return 0;
    }

private:
    ProgramOptions m_opts;

    void render(const std::vector<FileSummaryReport>& reports) {
        if (m_opts.format == OutputFormat::JSON) {
            ReportFormatter::renderJSON(std::wcout, reports);
        } else if (m_opts.format == OutputFormat::CSV) {
            ReportFormatter::renderCSV(std::wcout, reports);
        } else if (m_opts.format == OutputFormat::Detailed || (m_opts.format == OutputFormat::Auto && reports.size() == 1)) {
            for (const auto& r : reports) {
                ReportFormatter::renderDetailed(std::wcout, r, m_opts.humanReadable);
            }
        } else {
            ReportFormatter::renderTable(std::wcout, reports, m_opts.humanReadable);
        }
    }

    static void printVersion() {
        std::wcout << L"stat version 1.5.0\n";
        std::wcout << L"Copyright (C) 2026, Roberto J Dohnert\n";
    }

    static void printHelp() {
        std::wcout << LR"(stat(1)                 CrossShell for UNIX Reference Manual                  stat(1)

    NAME
        stat - display file status and classification information

    SYNOPSIS
        stat [OPTIONS] [FILE]...

    DESCRIPTION
        Displays file classifications, text line counts, Windows owner SIDs,
        and creation timestamps for each FILE. If no FILE arguments are specified,
        stat automatically reads whitespace- or newline-delimited file paths from
        standard input (stdin). Clean stream formatting allows direct piping into
        findstr, Select-String, jq, or downstream tools.

    OPTIONS
        -d, --detailed
            Display full detailed property cards for all files.

        -t, --table
            Force tabular display format.

        -b, --bytes
            Print exact byte counts instead of human-readable sizes.

        --json
            Emit results in JSON format for automated pipelines.

        --csv
            Emit results in CSV format for spreadsheets and data parsers.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        stat main.cpp
            Display detailed summary card for a single file.

        stat *.cpp *.h
            Display tabular summary of all C++ source files.

        stat --json app.log
            Output metadata in JSON structure.

        dir /b /s *.txt | stat
            Pipe directory listings directly into stat.

    CrossShell for UNIX                                                     stat(1)
    )";
    }
};

// ============================================================================
// Entry Point
// ============================================================================

int wmain(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    try {
        ProgramOptions options = CommandLineParser::parse(argc, argv);
        FileSummaryApplication app(options);
        return app.run();
    } catch (const std::exception& ex) {
        std::wcerr << L"stat: fatal error: " << ex.what() << L"\n";
        return 1;
    } catch (...) {
        std::wcerr << L"stat: unknown fatal error.\n";
        return 1;
    }
}