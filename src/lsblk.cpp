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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <winnetwk.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <io.h>
#include <memory>

#pragma comment(lib, "mpr.lib")

// ============================================================================
// 1. DATA MODELS
// ============================================================================

struct BlockDeviceRow {
    std::wstring name;
    std::wstring type;
    std::wstring fsType;
    std::wstring mountPoint;
    std::wstring size;
    std::wstring readOnly;
    std::wstring health;
};

enum class OutputFormat {
    Table,
    Csv,
    Json
};

// ============================================================================
// 2. STRING & FORMATTING HELPERS
// ============================================================================

class StringHelper {
public:
    static std::string Utf8Blk(const std::wstring& value) {
        int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(n), '\0');
        if (n) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
        return out;
    }

    static std::string CsvBlk(const std::string& value) {
        std::string out = "\"";
        for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
        return out + '"';
    }

    static std::string JsonBlk(const std::string& value) {
        std::string out;
        for (char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    }

    static std::wstring ToLowerCopy(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        return value;
    }

    static bool ContainsICase(const std::wstring& haystack, const std::wstring& needle) {
        if (needle.empty()) {
            return true;
        }
        return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::wstring::npos;
    }

    static std::wstring HumanSize(ULONGLONG bytes) {
        double value = static_cast<double>(bytes);
        const wchar_t* suffixes[] = { L"B", L"K", L"M", L"G", L"T", L"P" };
        size_t suffixIndex = 0;

        while (value >= 1024.0 && suffixIndex + 1 < (sizeof(suffixes) / sizeof(suffixes[0]))) {
            value /= 1024.0;
            ++suffixIndex;
        }

        wchar_t buffer[64] = {};
        if (suffixIndex == 0) {
            swprintf_s(buffer, L"%.0f%s", value, suffixes[suffixIndex]);
        } else if (value < 10.0) {
            swprintf_s(buffer, L"%.1f%s", value, suffixes[suffixIndex]);
        } else {
            swprintf_s(buffer, L"%.0f%s", value, suffixes[suffixIndex]);
        }
        return buffer;
    }
};

// ============================================================================
// 3. BLOCK DEVICE INSPECTION ENGINE
// ============================================================================

class BlockDeviceInspector {
public:
    static std::wstring HealthForDrive(const std::wstring& driveRoot) {
        if (driveRoot.size() < 2 || GetDriveTypeW(driveRoot.c_str()) == DRIVE_REMOTE ||
            GetDriveTypeW(driveRoot.c_str()) == DRIVE_CDROM) {
            return L"unknown";
        }

        std::wstring volumePath = L"\\\\.\\" + driveRoot.substr(0, 2);
        HANDLE volume = CreateFileW(
            volumePath.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (volume == INVALID_HANDLE_VALUE) {
            DWORD volumeFlags = 0;
            wchar_t fileSystemName[MAX_PATH] = {};
            return GetVolumeInformationW(
                       driveRoot.c_str(),
                       nullptr,
                       0,
                       nullptr,
                       nullptr,
                       &volumeFlags,
                       fileSystemName,
                       MAX_PATH)
                ? L"healthy"
                : L"unknown";
        }

        VOLUME_DISK_EXTENTS extents{};
        DWORD returned = 0;
        const BOOL gotExtents = DeviceIoControl(
            volume,
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            nullptr,
            0,
            &extents,
            sizeof(extents),
            &returned,
            nullptr);
        CloseHandle(volume);
        if (!gotExtents || extents.NumberOfDiskExtents == 0) {
            return L"healthy";
        }

        bool observedHealth = false;
        for (DWORD index = 0; index < extents.NumberOfDiskExtents && index < 1; ++index) {
            std::wstring diskPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(extents.Extents[index].DiskNumber);
            HANDLE disk = CreateFileW(
                diskPath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);
            if (disk == INVALID_HANDLE_VALUE) {
                continue;
            }

            STORAGE_PREDICT_FAILURE prediction{};
            returned = 0;
            if (DeviceIoControl(
                    disk,
                    IOCTL_STORAGE_PREDICT_FAILURE,
                    nullptr,
                    0,
                    &prediction,
                    sizeof(prediction),
                    &returned,
                    nullptr)) {
                observedHealth = true;
                CloseHandle(disk);
                if (prediction.PredictFailure != 0) {
                    return L"failing";
                }
                continue;
            }
            CloseHandle(disk);
        }

        if (observedHealth) {
            return L"healthy";
        }
        return L"healthy";
    }

    static std::wstring RemoteShareForDrive(const std::wstring& driveRoot) {
        if (driveRoot.size() < 2 || driveRoot[1] != L':') {
            return L"";
        }

        wchar_t localName[3] = { driveRoot[0], L':', L'\0' };
        wchar_t remoteName[MAX_PATH] = {};
        DWORD remoteLen = MAX_PATH;
        DWORD result = WNetGetConnectionW(localName, remoteName, &remoteLen);
        return (result == NO_ERROR) ? remoteName : L"";
    }

    static BlockDeviceRow BuildRow(const std::wstring& driveRoot) {
        BlockDeviceRow row;
        row.name = driveRoot.size() >= 2 ? driveRoot.substr(0, 2) : driveRoot;
        row.mountPoint = driveRoot;

        UINT driveType = GetDriveTypeW(driveRoot.c_str());
        switch (driveType) {
            case DRIVE_CDROM:
                row.type = L"rom";
                row.readOnly = L"yes";
                break;
            case DRIVE_REMOTE:
                row.type = L"disk";
                row.readOnly = L"no";
                break;
            case DRIVE_REMOVABLE:
            case DRIVE_FIXED:
            case DRIVE_RAMDISK:
            case DRIVE_UNKNOWN:
            default:
                row.type = L"disk";
                row.readOnly = L"no";
                break;
        }

        wchar_t fsName[MAX_PATH] = {};
        DWORD volumeFlags = 0;
        if (GetVolumeInformationW(driveRoot.c_str(), nullptr, 0, nullptr, nullptr, &volumeFlags, fsName, MAX_PATH)) {
            row.fsType = fsName;
            if ((volumeFlags & FILE_READ_ONLY_VOLUME) != 0) {
                row.readOnly = L"yes";
            }
        } else {
            row.fsType = L"unknown";
        }

        ULARGE_INTEGER freeBytesAvail = {};
        ULARGE_INTEGER totalBytes = {};
        ULARGE_INTEGER totalFreeBytes = {};
        if (GetDiskFreeSpaceExW(driveRoot.c_str(), &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
            row.size = StringHelper::HumanSize(totalBytes.QuadPart);
        } else {
            row.size = L"n/a";
        }
        row.health = HealthForDrive(driveRoot);

        if (driveType == DRIVE_REMOTE) {
            std::wstring remote = RemoteShareForDrive(driveRoot);
            if (!remote.empty()) {
                row.name = remote;
            }
        }

        return row;
    }

    static std::vector<BlockDeviceRow> EnumerateRows() {
        std::vector<BlockDeviceRow> rows;
        DWORD length = GetLogicalDriveStringsW(0, nullptr);
        if (length == 0) {
            return rows;
        }

        std::vector<wchar_t> buffer(length + 1, L'\0');
        if (GetLogicalDriveStringsW(length, buffer.data()) == 0) {
            return rows;
        }

        wchar_t* drive = buffer.data();
        while (*drive != L'\0') {
            rows.push_back(BuildRow(drive));
            drive += wcslen(drive) + 1;
        }

        return rows;
    }
};

// ============================================================================
// 4. OPTIONS & REPORTER
// ============================================================================

class LsblkOptions {
public:
    bool noHeadings = false;
    OutputFormat format = OutputFormat::Table;
    std::vector<std::wstring> filters;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    filters.push_back(argv[i] ? argv[i] : L"");
                }
                break;
            }
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"-V" || arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-n" || arg == L"--noheadings") {
                noHeadings = true;
                continue;
            }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"-") {
                std::wstring token;
                while (std::wcin >> token) filters.push_back(token);
                continue;
            }
            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"lsblk: unknown option -- " << arg << L"\n";
                return false;
            }
            filters.push_back(arg);
        }
        return true;
    }

    void PrintHelp(const wchar_t* progName) const {
           std::wcout << LR"(lsblk(1)                CrossShell for UNIX Reference Manual                   lsblk(1)

    NAME
        lsblk - list logical Windows block devices and mount points

    SYNOPSIS
        lsblk [OPTIONS] [FILTER...]

    DESCRIPTION
        Enumerates Windows logical drives, including remote drives, and reports
        filesystem, mount-point, size, read-only, and health information. Filters
        match NAME, TYPE, FSTYPE, MOUNTPOINT, SIZE, RO, or HEALTH.

    OPTIONS
        -n, --noheadings
            Suppress the table header row.

        --table
            Use aligned table output (default).

        --csv
            Emit CSV output.

        --json
            Emit a JSON array.

        -
            Read whitespace-delimited filters from standard input.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        --
            End options; remaining arguments are filters.

    OUTPUT
        Table and CSV fields are NAME, TYPE, FSTYPE, MOUNTPOINT, SIZE, RO, and
        HEALTH. JSON fields are name, type, fsType, mountPoint, size, readOnly,
        and health.

    EXAMPLES
        lsblk
            List all logical drives in table format.

        lsblk --json C:
            Emit the C: drive record as JSON.

        echo NTFS | lsblk -
            Read a filesystem filter from standard input.

    EXIT STATUS
        0          Help, version, or successful enumeration.
        1          Invalid option or failed argument parsing.

    CrossShell for UNIX                                                       lsblk(1)
    )";
    }

    void PrintVersion() const {
        std::wcout << L"lsblk 1.0.0\n";
    }
};

class LsblkReporter {
public:
    static void Report(const std::vector<BlockDeviceRow>& rows, const LsblkOptions& opts) {
        std::vector<BlockDeviceRow> selectedRows;
        for (const auto& row : rows) {
            bool keep = opts.filters.empty();
            for (const auto& filter : opts.filters) {
                if (StringHelper::ContainsICase(row.name, filter) ||
                    StringHelper::ContainsICase(row.type, filter) ||
                    StringHelper::ContainsICase(row.fsType, filter) ||
                    StringHelper::ContainsICase(row.mountPoint, filter) ||
                    StringHelper::ContainsICase(row.size, filter) ||
                    StringHelper::ContainsICase(row.readOnly, filter) ||
                    StringHelper::ContainsICase(row.health, filter)) {
                    keep = true;
                    break;
                }
            }
            if (keep) selectedRows.push_back(row);
        }

        if (opts.format == OutputFormat::Csv) {
            std::wcout << L"NAME,TYPE,FSTYPE,MOUNTPOINT,SIZE,RO,HEALTH\n";
            for (const auto& row : selectedRows) {
                std::cout << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.name)) << ','
                          << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.type)) << ','
                          << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.fsType)) << ','
                          << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.mountPoint)) << ','
                          << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.size)) << ','
                          << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.readOnly)) << ','
                          << StringHelper::CsvBlk(StringHelper::Utf8Blk(row.health)) << '\n';
            }
            return;
        }

        if (opts.format == OutputFormat::Json) {
            std::cout << "[\n";
            for (size_t i = 0; i < selectedRows.size(); ++i) {
                const auto& row = selectedRows[i];
                std::cout << "  {\"name\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.name))
                          << "\",\"type\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.type))
                          << "\",\"fsType\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.fsType))
                          << "\",\"mountPoint\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.mountPoint))
                          << "\",\"size\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.size))
                          << "\",\"readOnly\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.readOnly))
                          << "\",\"health\":\"" << StringHelper::JsonBlk(StringHelper::Utf8Blk(row.health))
                          << "\"}" << (i + 1 == selectedRows.size() ? "\n" : ",\n");
            }
            std::cout << "]\n";
            return;
        }

        if (!opts.noHeadings) {
            std::wcout << std::left
                       << std::setw(18) << L"NAME"
                       << std::setw(12) << L"TYPE"
                       << std::setw(14) << L"FSTYPE"
                       << std::setw(18) << L"MOUNTPOINT"
                       << std::setw(12) << L"SIZE"
                       << std::setw(12) << L"RO"
                       << L"HEALTH\n";
        }

        for (const auto& row : selectedRows) {
            std::wcout << std::left
                       << std::setw(18) << row.name
                       << std::setw(12) << row.type
                       << std::setw(14) << row.fsType
                       << std::setw(18) << row.mountPoint
                       << std::setw(12) << row.size
                       << std::setw(12) << row.readOnly
                       << row.health << L"\n";
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class LsblkApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        LsblkOptions opts;
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"lsblk";

        if (!opts.Parse(argc, argv)) {
            opts.PrintHelp(progName);
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp(progName);
            return 0;
        }
        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        auto rows = BlockDeviceInspector::EnumerateRows();
    std::wcout << L"\n";
        LsblkReporter::Report(rows, opts);
    std::wcout << L"\n";

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    LsblkApplication app;
    return app.Run(argc, argv);
}
