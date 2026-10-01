/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cwctype>

// ============================================================================
// Data Models & Options
// ============================================================================

enum class ScaleUnit {
    Bytes,
    KiloBinary,   // 1024 (1K)
    MegaBinary,   // 1024^2 (1M)
    GigaBinary,   // 1024^3 (1G)
    HumanBinary,  // K, M, G, T, P (1024-based)
    HumanDecimal, // k, M, G, T, P (1000-based / SI)
    CustomBlock
};

struct ProgramOptions {
    bool showAll = false;                  // -a, --all
    bool showLocalOnly = false;            // -l, --local
    bool printType = true;                 // -T, --print-type (shown by default)
    bool printTotal = false;               // --total
    bool showHelp = false;                 // -?, --help
    bool showVersion = false;              // -v, --version
    ScaleUnit scale = ScaleUnit::KiloBinary;
    uint64_t customBlockSize = 1024;
    std::wstring filterFsType;             // -t, --type
    std::wstring excludeFsType;            // -x, --exclude-type
    std::vector<std::wstring> targetPaths;
};

struct VolumeMetrics {
    std::wstring filesystemName; // Drive name / UNC / Device
    std::wstring fsType;         // NTFS, FAT32, ReFS, CSVFS, etc.
    std::wstring volumeLabel;
    std::wstring mountPoint;     // Drive root or mount folder
    UINT driveType = DRIVE_UNKNOWN;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    uint64_t availableBytes = 0; // Available to caller
    uint64_t usedBytes = 0;
    bool isAccessible = false;
};

// ============================================================================
// Formatting Utility
// ============================================================================

class SizeFormatter {
public:
    static std::wstring formatSize(uint64_t bytes, const ProgramOptions& opts) {
        switch (opts.scale) {
            case ScaleUnit::HumanBinary:
                return formatHuman(bytes, 1024.0, { L"B", L"K", L"M", L"G", L"T", L"P", L"E" });
            case ScaleUnit::HumanDecimal:
                return formatHuman(bytes, 1000.0, { L"B", L"k", L"M", L"G", L"T", L"P", L"E" });
            case ScaleUnit::KiloBinary:
                return std::to_wstring((bytes + 1023) / 1024);
            case ScaleUnit::MegaBinary:
                return std::to_wstring((bytes + (1024 * 1024 - 1)) / (1024 * 1024));
            case ScaleUnit::GigaBinary:
                return std::to_wstring((bytes + (1024ULL * 1024 * 1024 - 1)) / (1024ULL * 1024 * 1024));
            case ScaleUnit::CustomBlock: {
                uint64_t blk = opts.customBlockSize > 0 ? opts.customBlockSize : 1;
                return std::to_wstring((bytes + blk - 1) / blk);
            }
            default:
                return std::to_wstring(bytes);
        }
    }

    static std::wstring getBlockHeader(const ProgramOptions& opts) {
        switch (opts.scale) {
            case ScaleUnit::HumanBinary:
            case ScaleUnit::HumanDecimal:
                return L"Size";
            case ScaleUnit::KiloBinary:
                return L"1K-blocks";
            case ScaleUnit::MegaBinary:
                return L"1M-blocks";
            case ScaleUnit::GigaBinary:
                return L"1G-blocks";
            case ScaleUnit::CustomBlock:
                return std::to_wstring(opts.customBlockSize) + L"-blocks";
            default:
                return L"Blocks";
        }
    }

private:
    static std::wstring formatHuman(uint64_t bytes, double base, const std::vector<std::wstring>& units) {
        if (bytes == 0) return L"0" + units[0];

        double size = static_cast<double>(bytes);
        size_t unitIndex = 0;
        while (size >= base && unitIndex < units.size() - 1) {
            size /= base;
            unitIndex++;
        }

        std::wostringstream oss;
        if (unitIndex == 0) {
            oss << static_cast<uint64_t>(size) << units[unitIndex];
        } else if (size < 10.0) {
            oss << std::fixed << std::setprecision(1) << size << units[unitIndex];
        } else {
            oss << std::fixed << std::setprecision(0) << size << units[unitIndex];
        }
        return oss.str();
    }
};

// ============================================================================
// Volume Inspector (Win32 Interop Layer)
// ============================================================================

class VolumeInspector {
public:
    static std::optional<VolumeMetrics> queryPath(const std::wstring& rawPath) {
        WCHAR volumePath[MAX_PATH] = { 0 };
        if (!GetVolumePathNameW(rawPath.c_str(), volumePath, MAX_PATH)) {
            wcsncpy_s(volumePath, rawPath.c_str(), _TRUNCATE);
        }

        VolumeMetrics metrics;
        metrics.mountPoint = volumePath;
        metrics.filesystemName = volumePath;
        metrics.driveType = GetDriveTypeW(volumePath);

        WCHAR volumeName[MAX_PATH + 1] = { 0 };
        WCHAR fsName[MAX_PATH + 1] = { 0 };
        DWORD serial = 0, maxComponent = 0, flags = 0;

        if (GetVolumeInformationW(
                volumePath,
                volumeName, MAX_PATH + 1,
                &serial, &maxComponent, &flags,
                fsName, MAX_PATH + 1)) {
            metrics.volumeLabel = volumeName;
            metrics.fsType = fsName;
        } else {
            metrics.fsType = getDriveTypeString(metrics.driveType);
        }

        ULARGE_INTEGER freeBytesCaller, totalBytes, totalFree;
        if (GetDiskFreeSpaceExW(volumePath, &freeBytesCaller, &totalBytes, &totalFree)) {
            metrics.isAccessible = true;
            metrics.totalBytes = totalBytes.QuadPart;
            metrics.freeBytes = totalFree.QuadPart;
            metrics.availableBytes = freeBytesCaller.QuadPart;
            metrics.usedBytes = (metrics.totalBytes >= metrics.freeBytes) 
                                ? (metrics.totalBytes - metrics.freeBytes) : 0;
        } else {
            metrics.isAccessible = false;
        }

        return metrics;
    }

    static std::vector<VolumeMetrics> enumerateAllVolumes() {
        std::vector<VolumeMetrics> volumes;
        DWORD bufferLength = GetLogicalDriveStringsW(0, nullptr);
        if (bufferLength == 0) return volumes;

        std::vector<WCHAR> buffer(bufferLength + 1);
        GetLogicalDriveStringsW(bufferLength, buffer.data());

        const WCHAR* current = buffer.data();
        while (*current) {
            std::wstring driveRoot = current;
            auto result = queryPath(driveRoot);
            if (result.has_value()) {
                volumes.push_back(result.value());
            }
            current += driveRoot.length() + 1;
        }

        return volumes;
    }

    static std::wstring getDriveTypeString(UINT driveType) {
        switch (driveType) {
            case DRIVE_FIXED:     return L"Local Disk";
            case DRIVE_REMOVABLE: return L"Removable";
            case DRIVE_REMOTE:    return L"Network";
            case DRIVE_CDROM:     return L"CD-ROM";
            case DRIVE_RAMDISK:   return L"RAM Disk";
            default:              return L"Unknown";
        }
    }
};

// ============================================================================
// Table Formatter & Console Renderer
// ============================================================================

class TableRenderer {
public:
    struct Row {
        std::vector<std::wstring> cells;
    };

    void addHeader(const std::vector<std::wstring>& headers) {
        m_headers = headers;
    }

    void addRow(const std::vector<std::wstring>& row) {
        m_rows.push_back({ row });
    }

    void render(std::wostream& os) const {
        if (m_headers.empty()) return;

        size_t colCount = m_headers.size();
        std::vector<size_t> colWidths(colCount, 0);

        for (size_t i = 0; i < colCount; ++i) {
            colWidths[i] = m_headers[i].length();
        }

        for (const auto& row : m_rows) {
            for (size_t i = 0; i < std::min(row.cells.size(), colCount); ++i) {
                colWidths[i] = std::max(colWidths[i], row.cells[i].length());
            }
        }

        for (size_t i = 0; i < colCount; ++i) {
            bool rightAlign = isNumericColumn(i);
            printCell(os, m_headers[i], colWidths[i], rightAlign, i == colCount - 1);
        }
        os << L"\n";

        for (const auto& row : m_rows) {
            for (size_t i = 0; i < colCount; ++i) {
                std::wstring val = (i < row.cells.size()) ? row.cells[i] : L"";
                bool rightAlign = isNumericColumn(i);
                printCell(os, val, colWidths[i], rightAlign, i == colCount - 1);
            }
            os << L"\n";
        }
    }

private:
    std::vector<std::wstring> m_headers;
    std::vector<Row> m_rows;

    bool isNumericColumn(size_t index) const {
        if (index == 0) return false;
        if (index == m_headers.size() - 1) return false;
        if (m_headers[index] == L"Type") return false;
        return true;
    }

    static void printCell(std::wostream& os, const std::wstring& text, size_t width, bool rightAlign, bool isLast) {
        if (isLast && !rightAlign) {
            os << text;
            return;
        }
        if (rightAlign) {
            os << std::setw(static_cast<int>(width)) << text;
        } else {
            os << std::left << std::setw(static_cast<int>(width)) << text << std::right;
        }
        if (!isLast) os << L"  ";
    }
};

// ============================================================================
// Command Line Parser
// ============================================================================

class CommandLineParser {
public:
    static ProgramOptions parse(int argc, wchar_t* argv[]) {
        ProgramOptions opts;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--human-readable") {
                opts.scale = ScaleUnit::HumanBinary;
            } else if (arg == L"-H" || arg == L"--si") {
                opts.scale = ScaleUnit::HumanDecimal;
            } else if (arg == L"-k") {
                opts.scale = ScaleUnit::KiloBinary;
            } else if (arg == L"-m") {
                opts.scale = ScaleUnit::MegaBinary;
            } else if (arg == L"-g" || arg == L"-G") {
                opts.scale = ScaleUnit::GigaBinary;
            } else if (arg == L"-a" || arg == L"--all") {
                opts.showAll = true;
            } else if (arg == L"-l" || arg == L"--local") {
                opts.showLocalOnly = true;
            } else if (arg == L"-T" || arg == L"--print-type") {
                opts.printType = true;
            } else if (arg == L"--total") {
                opts.printTotal = true;
            } else if (arg == L"--help" || arg == L"-?") {
                opts.showHelp = true;
            } else if (arg == L"-v" || arg == L"--version") {
                opts.showVersion = true;
            } else if (arg.rfind(L"-B", 0) == 0 || arg.rfind(L"--block-size", 0) == 0) {
                opts.scale = ScaleUnit::CustomBlock;
                std::wstring val;
                if (arg.rfind(L"-B", 0) == 0 && arg.length() > 2) {
                    val = arg.substr(2);
                } else if (arg.rfind(L"--block-size=", 0) == 0) {
                    val = arg.substr(13);
                } else if (i + 1 < argc) {
                    val = argv[++i];
                }
                opts.customBlockSize = parseBlockSize(val);
            } else if (arg.rfind(L"-t", 0) == 0 || arg.rfind(L"--type", 0) == 0) {
                if (arg.rfind(L"--type=", 0) == 0) {
                    opts.filterFsType = arg.substr(7);
                } else if (i + 1 < argc) {
                    opts.filterFsType = argv[++i];
                }
            } else if (arg.rfind(L"-x", 0) == 0 || arg.rfind(L"--exclude-type", 0) == 0) {
                if (arg.rfind(L"--exclude-type=", 0) == 0) {
                    opts.excludeFsType = arg.substr(15);
                } else if (i + 1 < argc) {
                    opts.excludeFsType = argv[++i];
                }
            } else if (arg.length() > 0 && arg[0] == L'-' && arg.length() > 1 && arg[1] != L'-') {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case L'h': opts.scale = ScaleUnit::HumanBinary; break;
                        case L'H': opts.scale = ScaleUnit::HumanDecimal; break;
                        case L'k': opts.scale = ScaleUnit::KiloBinary; break;
                        case L'm': opts.scale = ScaleUnit::MegaBinary; break;
                        case L'a': opts.showAll = true; break;
                        case L'l': opts.showLocalOnly = true; break;
                        case L'T': opts.printType = true; break;
                        case L'?': opts.showHelp = true; break;
                        default:
                            std::wcerr << L"bdf: unrecognized option -- '" << arg[c] << L"'\n";
                            std::wcerr << L"Try 'bdf --help' or 'bdf -?' for more information.\n";
                            exit(1);
                    }
                }
            } else {
                opts.targetPaths.push_back(arg);
            }
        }
        return opts;
    }

private:
    static uint64_t parseBlockSize(const std::wstring& val) {
        if (val.empty()) return 1024;
        try {
            size_t idx = 0;
            uint64_t size = std::stoull(val, &idx);
            if (idx < val.length()) {
                wchar_t unit = val[idx];
                switch (unit) {
                    case L'K': case L'k': size *= 1024ULL; break;
                    case L'M': case L'm': size *= (1024ULL * 1024); break;
                    case L'G': case L'g': size *= (1024ULL * 1024 * 1024); break;
                    case L'T': case L't': size *= (1024ULL * 1024 * 1024 * 1024); break;
                    default: break;
                }
            }
            return size > 0 ? size : 1024;
        } catch (...) {
            return 1024;
        }
    }
};

// ============================================================================
// Piping Support & Standard Input Reader
// ============================================================================

class PipelineManager {
public:
    static bool isInputPiped() {
        return _isatty(_fileno(stdin)) == 0;
    }

    static std::vector<std::wstring> readPipedPaths() {
        std::vector<std::wstring> paths;
        std::wstring token;
        while (std::wcin >> token) {
            if (!token.empty()) {
                paths.push_back(token);
            }
        }
        return paths;
    }
};

// ============================================================================
// Core Application Controller
// ============================================================================

class DiskFreeApplication {
public:
    explicit DiskFreeApplication(ProgramOptions options)
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

        if (m_opts.targetPaths.empty() && PipelineManager::isInputPiped()) {
            auto piped = PipelineManager::readPipedPaths();
            m_opts.targetPaths.insert(m_opts.targetPaths.end(), piped.begin(), piped.end());
        }

        std::vector<VolumeMetrics> volumes;
        if (m_opts.targetPaths.empty()) {
            volumes = VolumeInspector::enumerateAllVolumes();
        } else {
            for (const auto& path : m_opts.targetPaths) {
                auto vol = VolumeInspector::queryPath(path);
                if (vol.has_value()) {
                    volumes.push_back(vol.value());
                } else {
                    std::wcerr << L"bdf: '" << path << L"': No such file or directory\n";
                }
            }
        }

        displayMetrics(volumes);
        return 0;
    }

private:
    ProgramOptions m_opts;

    bool matchesFilter(const VolumeMetrics& vol) const {
        if (!m_opts.showAll && (!vol.isAccessible || vol.driveType == DRIVE_CDROM || vol.totalBytes == 0)) {
            return false;
        }
        if (m_opts.showLocalOnly && vol.driveType == DRIVE_REMOTE) {
            return false;
        }
        if (!m_opts.filterFsType.empty() && !equalIgnoreCase(vol.fsType, m_opts.filterFsType)) {
            return false;
        }
        if (!m_opts.excludeFsType.empty() && equalIgnoreCase(vol.fsType, m_opts.excludeFsType)) {
            return false;
        }
        return true;
    }

    void displayMetrics(const std::vector<VolumeMetrics>& volumes) const {
        TableRenderer table;

        std::vector<std::wstring> headers = { L"Filesystem", L"Type" };
        headers.push_back(SizeFormatter::getBlockHeader(m_opts));
        headers.push_back(L"Used");
        headers.push_back(L"Available");
        headers.push_back(L"Use%");
        headers.push_back(L"Mounted on");

        table.addHeader(headers);

        uint64_t grandTotal = 0;
        uint64_t grandUsed = 0;
        uint64_t grandAvail = 0;
        size_t displayedCount = 0;

        for (const auto& vol : volumes) {
            if (!matchesFilter(vol)) continue;

            displayedCount++;
            grandTotal += vol.totalBytes;
            grandUsed += vol.usedBytes;
            grandAvail += vol.availableBytes;

            std::wstring usePercent = L"-";
            if (vol.totalBytes > 0) {
                double pct = (static_cast<double>(vol.usedBytes) / static_cast<double>(vol.totalBytes)) * 100.0;
                usePercent = std::to_wstring(static_cast<int>(std::ceil(pct))) + L"%";
            }

            std::vector<std::wstring> row;
            row.push_back(vol.filesystemName);
            row.push_back(vol.fsType.empty() ? L"Unknown" : vol.fsType);

            if (vol.isAccessible) {
                row.push_back(SizeFormatter::formatSize(vol.totalBytes, m_opts));
                row.push_back(SizeFormatter::formatSize(vol.usedBytes, m_opts));
                row.push_back(SizeFormatter::formatSize(vol.availableBytes, m_opts));
                row.push_back(usePercent);
            } else {
                row.push_back(L"-");
                row.push_back(L"-");
                row.push_back(L"-");
                row.push_back(L"-");
            }

            std::wstring mountDesc = vol.mountPoint;
            if (!vol.volumeLabel.empty()) {
                mountDesc += L" [" + vol.volumeLabel + L"]";
            }
            row.push_back(mountDesc);

            table.addRow(row);
        }

        if (m_opts.printTotal && displayedCount > 0) {
            std::vector<std::wstring> totalRow;
            totalRow.push_back(L"total");
            totalRow.push_back(L"-");

            totalRow.push_back(SizeFormatter::formatSize(grandTotal, m_opts));
            totalRow.push_back(SizeFormatter::formatSize(grandUsed, m_opts));
            totalRow.push_back(SizeFormatter::formatSize(grandAvail, m_opts));

            std::wstring totalUsePercent = L"-";
            if (grandTotal > 0) {
                double pct = (static_cast<double>(grandUsed) / static_cast<double>(grandTotal)) * 100.0;
                totalUsePercent = std::to_wstring(static_cast<int>(std::ceil(pct))) + L"%";
            }
            totalRow.push_back(totalUsePercent);
            totalRow.push_back(L"-");

            table.addRow(totalRow);
        }

        table.render(std::wcout);
    }

    static bool equalIgnoreCase(const std::wstring& a, const std::wstring& b) {
        if (a.length() != b.length()) return false;
        return std::equal(a.begin(), a.end(), b.begin(), [](wchar_t c1, wchar_t c2) {
            return std::towlower(c1) == std::towlower(c2);
        });
    }

    static void printVersion() {
        std::wcout << L"bdf version 5.1.0\n";
        std::wcout << L"Copyright (C) 2026, Roberto J Dohnert.\n";
    }

    static void printHelp() {
        std::wcout << LR"(bdf(1)                  CrossShell for UNIX Reference Manual                   bdf(1)

    NAME
        bdf - display amount of free disk space on mounted filesystems

    SYNOPSIS
        bdf [OPTIONS] [FILE...]

    DESCRIPTION
        bdf displays the amount of disk space available on the filesystem
        containing each specified FILE or on all currently mounted filesystems
        by default.

    OPTIONS
        -a, --all
            Include dummy and inaccessible filesystems.

        -B, --block-size=SIZE
            Scale sizes by SIZE before printing (e.g., -BM).

        -h, --human-readable
            Print sizes in powers of 1024 (e.g., 1023M, 24G).

        -H, --si
            Print sizes in powers of 1000 (e.g., 1.1G, 15k).

        -k
            Like --block-size=1K (default).

        -m
            Like --block-size=1M.

        -g, -G
            Like --block-size=1G.

        -l, --local
            Limit listing to local filesystems (excludes network shares).

        -T, --print-type
            Print filesystem type (NTFS, FAT32, ReFS, CSVFS).

        -t, --type=TYPE
            Limit listing to filesystems of type TYPE.

        -x, --exclude-type=T
            Limit listing to filesystems not of type T.

        --total
            Produce a grand total row at the end.

        -h, --help, -?
            Display this reference manual.

        -v, --version
            Output version information and exit.

    EXAMPLES
        bdf -h
            Print all accessible mounted volumes in human-readable units.

        bdf -h -T
            Include filesystem types in report.

        bdf -h --total
            Display capacity metrics with a total summary row.

        bdf -t NTFS
            Display only NTFS filesystems.

    CrossShell for UNIX                                                    bdf(1)
)";
    }
};

// ============================================================================
// Entry Point
// ============================================================================

int wmain(int argc, wchar_t* argv[]) {
    bool stdoutConsole = _isatty(_fileno(stdout)) != 0;
    bool stderrConsole = _isatty(_fileno(stderr)) != 0;
    _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stderr), stderrConsole ? _O_U16TEXT : _O_U8TEXT);

    try {
        ProgramOptions options = CommandLineParser::parse(argc, argv);
        DiskFreeApplication app(options);
        return app.run();
    } catch (const std::exception& ex) {
        std::wcerr << L"bdf: fatal error: " << ex.what() << L"\n";
        return 1;
    } catch (...) {
        std::wcerr << L"bdf: unknown fatal error occurred.\n";
        return 1;
    }
}