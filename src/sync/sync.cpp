/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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
#include <shellapi.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

// ============================================================================
// Privilege and System Utilities
// ============================================================================

class SystemSecurity {
public:
    static bool is_elevated() {
        bool elevated = false;
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION elevation;
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
                elevated = (elevation.TokenIsElevated != 0);
            }
            CloseHandle(hToken);
        }
        return elevated;
    }
};

// ============================================================================
// Abstract Target Interface (OOP Design)
// ============================================================================

class ISyncTarget {
public:
    virtual ~ISyncTarget() = default;
    virtual bool flush(std::string& error_msg) = 0;
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
};

// Target representing a physical/logical Windows Volume (e.g., \\.\C:)
class VolumeTarget : public ISyncTarget {
private:
    std::wstring root_path;     // e.g. L"C:\\"
    std::wstring device_path;   // e.g. L"\\\\.\\C:"
    std::string drive_letter;   // e.g. "C:"
    std::string fs_name;        // e.g. "NTFS"
    std::string volume_label;   // e.g. "System"
    UINT drive_type;            // DRIVE_FIXED, DRIVE_REMOVABLE, etc.

public:
    VolumeTarget(wchar_t letter, UINT type) : drive_type(type) {
        drive_letter = std::string(1, static_cast<char>(letter)) + ":";
        root_path = std::wstring(1, letter) + L":\\";
        device_path = L"\\\\.\\" + std::wstring(1, letter) + L":";

        wchar_t vol_name[MAX_PATH + 1] = {0};
        wchar_t file_system[MAX_PATH + 1] = {0};
        if (GetVolumeInformationW(root_path.c_str(), vol_name, MAX_PATH, NULL, NULL, NULL, file_system, MAX_PATH)) {
            char mb_vol[MAX_PATH * 2] = {0};
            char mb_fs[MAX_PATH * 2] = {0};
            WideCharToMultiByte(CP_UTF8, 0, vol_name, -1, mb_vol, sizeof(mb_vol), NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, file_system, -1, mb_fs, sizeof(mb_fs), NULL, NULL);
            volume_label = mb_vol;
            fs_name = mb_fs;
        }
    }

    std::string get_name() const override {
        return drive_letter;
    }

    std::string get_description() const override {
        std::string type_str = "Fixed";
        if (drive_type == DRIVE_REMOVABLE) type_str = "Removable";
        else if (drive_type == DRIVE_CDROM) type_str = "Optical";
        else if (drive_type == DRIVE_REMOTE) type_str = "Network";
        else if (drive_type == DRIVE_RAMDISK) type_str = "RAMDisk";

        std::string desc = type_str;
        if (!fs_name.empty()) desc += ", " + fs_name;
        if (!volume_label.empty()) desc += " [" + volume_label + "]";
        return desc;
    }

    UINT get_drive_type() const { return drive_type; }

    bool flush(std::string& error_msg) override {
        // Opening a volume handle with GENERIC_READ | GENERIC_WRITE requires admin rights
        HANDLE hVolume = CreateFileW(
            device_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hVolume == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                error_msg = "Access denied (requires Administrator privileges)";
            } else if (err == ERROR_NOT_READY) {
                error_msg = "Device not ready";
            } else {
                error_msg = "Win32 Error " + std::to_string(err);
            }
            return false;
        }

        BOOL success = FlushFileBuffers(hVolume);
        if (!success) {
            DWORD err = GetLastError();
            error_msg = "FlushFileBuffers failed: Win32 Error " + std::to_string(err);
            CloseHandle(hVolume);
            return false;
        }

        CloseHandle(hVolume);
        return true;
    }
};

// Target representing a single file or directory
class FileTarget : public ISyncTarget {
private:
    std::string path_str;
    std::wstring path_wstr;
    bool sync_containing_filesystem;

public:
    FileTarget(std::string path, bool file_system) 
        : path_str(std::move(path)), sync_containing_filesystem(file_system) {
        int len = MultiByteToWideChar(CP_UTF8, 0, path_str.c_str(), -1, NULL, 0);
        path_wstr.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, path_str.c_str(), -1, &path_wstr[0], len);
    }

    std::string get_name() const override {
        return path_str;
    }

    std::string get_description() const override {
        return sync_containing_filesystem ? "Containing File System" : "File Data Stream";
    }

    bool flush(std::string& error_msg) override {
        if (sync_containing_filesystem) {
            // Find root volume path of this file/folder
            wchar_t volume_path[MAX_PATH + 1] = {0};
            if (!GetVolumePathNameW(path_wstr.c_str(), volume_path, MAX_PATH)) {
                error_msg = "Cannot resolve parent volume for path";
                return false;
            }
            if (wcslen(volume_path) >= 2 && volume_path[1] == L':') {
                VolumeTarget parent_vol(volume_path[0], GetDriveTypeW(volume_path));
                return parent_vol.flush(error_msg);
            }
            error_msg = "Unsupported volume path format";
            return false;
        }

        // Open individual file handle to flush its specific data buffers
        HANDLE hFile = CreateFileW(
            path_wstr.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            error_msg = (err == ERROR_ACCESS_DENIED) ? "Access denied" : "Win32 Error " + std::to_string(err);
            return false;
        }

        BOOL success = FlushFileBuffers(hFile);
        if (!success) {
            DWORD err = GetLastError();
            error_msg = "FlushFileBuffers failed: Win32 Error " + std::to_string(err);
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }
};

// ============================================================================
// Storage Discovery Manager
// ============================================================================

class StorageManager {
public:
    static std::vector<std::unique_ptr<VolumeTarget>> discover_volumes(bool removable_only) {
        std::vector<std::unique_ptr<VolumeTarget>> volumes;
        DWORD drivesMask = GetLogicalDrives();

        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            if (drivesMask & (1 << (letter - 'A'))) {
                std::wstring root = std::wstring(1, letter) + L":\\";
                UINT driveType = GetDriveTypeW(root.c_str());

                // Skip non-local/non-disk drives
                if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_REMOTE) {
                    continue;
                }

                if (removable_only && driveType != DRIVE_REMOVABLE) {
                    continue;
                }

                volumes.push_back(std::make_unique<VolumeTarget>(letter, driveType));
            }
        }
        return volumes;
    }
};

// ============================================================================
// CLI Options and Parser
// ============================================================================

struct CliOptions {
    bool file_system = false;   // -f, --file-system
    bool data_only = false;     // -d, --data
    bool removable_only = false;// -r, --removable
    bool verbose = false;       // -v, --verbose
    bool quiet = false;         // -q, --quiet
    bool show_help = false;     // -h, --help
    bool show_version = false;  // -V, --version
    std::vector<std::string> targets;
};

class CliParser {
public:
    static bool parse(int argc, char* argv[], CliOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                opts.show_help = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                opts.show_version = true;
                return true;
            } else if (arg == "-f" || arg == "--file-system") {
                opts.file_system = true;
            } else if (arg == "-d" || arg == "--data") {
                opts.data_only = true;
            } else if (arg == "-r" || arg == "--removable") {
                opts.removable_only = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (!arg.empty() && arg[0] == '-') {
                // Short flags cluster (e.g. -rv)
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    if (c == 'f') opts.file_system = true;
                    else if (c == 'd') opts.data_only = true;
                    else if (c == 'r') opts.removable_only = true;
                    else if (c == 'v') opts.verbose = true;
                    else if (c == 'q') opts.quiet = true;
                    else if (c == 'h') { opts.show_help = true; return true; }
                    else if (c == 'V') { opts.show_version = true; return true; }
                    else {
                        std::cerr << "sync: unknown option -- '" << c << "'\n";
                        std::cerr << "Try 'sync --help' for more information.\n";
                        return false;
                    }
                }
            } else {
                opts.targets.push_back(arg);
            }
        }
        return true;
    }
};

// ============================================================================
// Comprehensive BSD Help Documentation
// ============================================================================

const char* const BSD_MANUAL =
R"(NAME
     sync -- force completion of pending disk writes (flush cache)

SYNOPSIS
     sync [-dfhqrsvV] [file ...]

DESCRIPTION
     The sync utility forces a write of dirty (modified) buffers in the block
     buffer cache out to the underlying physical storage media. On Windows,
     sync coordinates with the Windows Cache Manager and storage subsystem
     to commit dirty cache pages across mounted NTFS, ReFS, FAT32, and exFAT
     volumes via FlushFileBuffers.

     If no files or paths are specified, all active fixed and removable disk
     volumes on the system are synchronized.

OPTIONS
     -d, --data
             Synchronize only file data without metadata updates where
             supported.

     -f, --file-system
             Synchronize the entire file system(s) containing the specified
             files or directories.

     -r, --removable
             Restrict cache synchronization to removable storage media
             (e.g., USB thumb drives, external disks, flash memory cards).

     -v, --verbose
             Verbose mode. Displays diagnostic details for each synchronized
             drive or file, including volume label, file system, and status.

     -q, --quiet
             Quiet mode. Suppress all informational output and warnings.

     -h, --help
             Display this manual help page and exit.

     -V, --version
             Display version information and exit.

OPERANDS
     file ...
             Synchronize the specified files or directory paths. If -f is also
             specified, the volume containing each path is synchronized.

EXIT STATUS
     0       All buffers were successfully synchronized.
     1       One or more volumes or files failed to synchronize, or
             permission was denied.
     2       Invalid command-line arguments.

COMPATIBILITY NOTES

     Note: Flushing entire volume caches requires an elevated Command Prompt
     or PowerShell terminal (Run as Administrator) under Windows UAC.

SEE ALSO
     fsync(2), sync(2), reboot(8), halt(8)
)";

// ============================================================================
// Core Execution Controller
// ============================================================================

class SyncApp {
private:
    CliOptions options;

public:
    int run(int argc, char* argv[]) {
        if (!CliParser::parse(argc, argv, options)) {
            return 2;
        }

        if (options.show_help) {
            std::cout << BSD_MANUAL;
            return 0;
        }

        if (options.show_version) {
            std::cout << "sync 2.0\n";
            return 0;
        }

        bool elevated = SystemSecurity::is_elevated();
        if (!elevated && !options.quiet) {
            std::cerr << "sync: warning: flushing disk volumes requires Administrator privileges.\n"
                      << "      If writes fail, re-run from an elevated console.\n";
        }

        // Flush standard runtime streams first (POSIX compatibility)
        _flushall();

        std::vector<std::unique_ptr<ISyncTarget>> targets;

        if (options.targets.empty()) {
            // Default BSD behavior: Synchronize all logical disk drives
            auto vols = StorageManager::discover_volumes(options.removable_only);
            for (auto& v : vols) {
                targets.push_back(std::move(v));
            }
        } else {
            // Synchronize specified files / paths
            for (const auto& path : options.targets) {
                targets.push_back(std::make_unique<FileTarget>(path, options.file_system));
            }
        }

        if (targets.empty()) {
            if (!options.quiet) {
                std::cout << "sync: no matching storage targets found.\n";
            }
            return 0;
        }

        int failed_count = 0;
        auto start_time = std::chrono::steady_clock::now();

        for (const auto& target : targets) {
            std::string err;
            bool ok = target->flush(err);

            if (!ok) {
                failed_count++;
                if (!options.quiet) {
                    std::cerr << "sync: " << target->get_name() << ": " << err << "\n";
                }
            } else if (options.verbose) {
                std::cout << "sync: " << std::left << std::setw(6) << target->get_name()
                          << " (" << target->get_description() << ") ... OK\n";
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        if (options.verbose) {
            std::cout << "sync: completed in " << elapsed << " ms ("
                      << (targets.size() - failed_count) << "/" << targets.size() 
                      << " targets committed to disk)\n";
        }

        return (failed_count == 0) ? 0 : 1;
    }
};

// ============================================================================
// Windows Console Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SyncApp app;
    return app.run(argc, argv);
}