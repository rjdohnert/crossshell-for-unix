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
#include <winnetwk.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

#pragma comment(lib, "mpr.lib")

// ============================================================================
// 1. RAII GUARDS & DATA MODELS
// ============================================================================

class ScopedWpOpen {
public:
    explicit ScopedWpOpen(FILE* pipe = nullptr) : m_pipe(pipe) {}
    ~ScopedWpOpen() { Close(); }

    ScopedWpOpen(const ScopedWpOpen&) = delete;
    ScopedWpOpen& operator=(const ScopedWpOpen&) = delete;

    ScopedWpOpen(ScopedWpOpen&& other) noexcept : m_pipe(other.m_pipe) { other.m_pipe = nullptr; }
    ScopedWpOpen& operator=(ScopedWpOpen&& other) noexcept {
        if (this != &other) {
            Close();
            m_pipe = other.m_pipe;
            other.m_pipe = nullptr;
        }
        return *this;
    }

    FILE* Get() const { return m_pipe; }
    bool IsValid() const { return m_pipe != nullptr; }

    void Close() {
        if (m_pipe) {
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

private:
    FILE* m_pipe;
};

struct MountEntry {
    std::wstring source;
    std::wstring target;
    std::wstring fsType;
    std::wstring options;
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

// ============================================================================
// 2. STRING & FORMATTING HELPERS
// ============================================================================

class StringHelper {
public:
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

    static std::string ToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static std::wstring EscapeJson(const std::wstring& text) {
        std::wstring result;
        for (wchar_t ch : text) {
            if (ch == L'\\') result += L"\\\\";
            else if (ch == L'\"') result += L"\\\"";
            else result += ch;
        }
        return result;
    }
};

// ============================================================================
// 3. MOUNT INSPECTION ENGINE
// ============================================================================

class MountInspector {
public:
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

    static std::wstring JoinOptions(UINT driveType, DWORD volumeFlags) {
        std::wstring options = (driveType == DRIVE_CDROM || (volumeFlags & FILE_READ_ONLY_VOLUME)) ? L"ro" : L"rw";
        switch (driveType) {
            case DRIVE_REMOTE:    options += L",network"; break;
            case DRIVE_CDROM:     options += L",cdrom"; break;
            case DRIVE_REMOVABLE: options += L",removable"; break;
            default:              options += L",local"; break;
        }
        return options;
    }

    static MountEntry BuildEntry(const std::wstring& driveRoot) {
        MountEntry entry;
        entry.target = driveRoot;
        entry.source = driveRoot.size() >= 2 ? driveRoot.substr(0, 2) : driveRoot;

        UINT driveType = GetDriveTypeW(driveRoot.c_str());
        if (driveType == DRIVE_REMOTE) {
            std::wstring remote = RemoteShareForDrive(driveRoot);
            if (!remote.empty()) {
                entry.source = remote;
            }
        } else {
            wchar_t guidPath[MAX_PATH] = {};
            if (GetVolumeNameForVolumeMountPointW(driveRoot.c_str(), guidPath, MAX_PATH)) {
                entry.source = guidPath;
            }
        }

        wchar_t fsName[MAX_PATH] = {};
        DWORD volumeFlags = 0;
        if (GetVolumeInformationW(driveRoot.c_str(), nullptr, 0, nullptr, nullptr, &volumeFlags, fsName, MAX_PATH)) {
            entry.fsType = fsName;
        } else {
            entry.fsType = L"unknown";
        }

        entry.options = JoinOptions(driveType, volumeFlags);
        return entry;
    }

    static std::vector<MountEntry> EnumerateMounts() {
        std::vector<MountEntry> entries;
        DWORD length = GetLogicalDriveStringsW(0, nullptr);
        if (length == 0) {
            return entries;
        }

        std::vector<wchar_t> buffer(length + 1, L'\0');
        if (GetLogicalDriveStringsW(length, buffer.data()) == 0) {
            return entries;
        }

        wchar_t* drive = buffer.data();
        while (*drive != L'\0') {
            entries.push_back(BuildEntry(drive));
            drive += wcslen(drive) + 1;
        }

        return entries;
    }
};

// ============================================================================
// 4. OPTIONS & REPORTER
// ============================================================================

class FindmntOptions {
public:
    bool noHeadings = false;
    OutputFormat outputFormat = OutputFormat::Default;
    std::wstring pipeCommand;
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
            if (arg == L"--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == L"--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == L"--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"findmnt: unknown option -- " << arg << L"\n";
                return false;
            }
            filters.push_back(arg);
        }
        return true;
    }

    void PrintHelp(const wchar_t* progName) const {
        std::wcout << LR"(findmnt(1)              CrossShell for UNIX Reference Manual               findmnt(1)

    NAME
        findmnt - find and inspect mounted filesystems and drive volumes

    SYNOPSIS
        findmnt [OPTIONS] [DEVICE|MOUNTPOINT]

    DESCRIPTION
        findmnt lists all mounted filesystems or searches for a filesystem in
        Windows drive tables, network mounts, and volume mount points.

    OPTIONS
        -n, --noheadings
            Do not print a header line.

        -l, --list
            Use list output format.

        -t, --types TYPES
            Limit the set of printed filesystems by filesystem types.

        --json, --csv, --table
            Output filesystem mount information as JSON, CSV, or table.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        findmnt
            List all active mounts and drive volumes.

        findmnt -t NTFS --json
            List all NTFS mounted volumes in JSON format.

    CrossShell for UNIX                                                findmnt(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"findmnt 1.0.0\n";
    }
};

class FindmntReporter {
public:
    static void Report(const std::vector<MountEntry>& entries, const FindmntOptions& opts) {
        std::wstring output;
        size_t jsonIndex = 0;

        if (opts.outputFormat == OutputFormat::Default && opts.pipeCommand.empty() && !opts.noHeadings) {
            std::wcout << std::left
                       << std::setw(42) << L"SOURCE"
                       << std::setw(14) << L"TARGET"
                       << std::setw(14) << L"FSTYPE"
                       << L"OPTIONS\n";
        }

        for (const auto& entry : entries) {
            bool keep = opts.filters.empty();
            for (const auto& filter : opts.filters) {
                if (StringHelper::ContainsICase(entry.source, filter) ||
                    StringHelper::ContainsICase(entry.target, filter) ||
                    StringHelper::ContainsICase(entry.fsType, filter) ||
                    StringHelper::ContainsICase(entry.options, filter)) {
                    keep = true;
                    break;
                }
            }

            if (!keep) {
                continue;
            }

            if (opts.outputFormat == OutputFormat::Json) {
                if (jsonIndex++ != 0) output += L",\n";
                output += L"{\"source\":\"" + StringHelper::EscapeJson(entry.source) + 
                          L"\",\"target\":\"" + StringHelper::EscapeJson(entry.target) + 
                          L"\",\"fstype\":\"" + StringHelper::EscapeJson(entry.fsType) + 
                          L"\",\"options\":\"" + StringHelper::EscapeJson(entry.options) + L"\"}";
            } else if (opts.outputFormat == OutputFormat::Csv) {
                output += entry.source + L"," + entry.target + L"," + entry.fsType + L"," + entry.options + L"\n";
            } else if (opts.outputFormat == OutputFormat::Table || !opts.pipeCommand.empty()) {
                output += entry.source + L"\t" + entry.target + L"\t" + entry.fsType + L"\t" + entry.options + L"\n";
            } else {
                std::wcout << std::left
                           << std::setw(42) << entry.source
                           << std::setw(14) << entry.target
                           << std::setw(14) << entry.fsType
                           << entry.options << L"\n";
            }
        }

        if (opts.outputFormat != OutputFormat::Default || !opts.pipeCommand.empty()) {
            if (opts.outputFormat == OutputFormat::Json) output = L"[\n" + output + L"\n]\n";
            else if (opts.outputFormat == OutputFormat::Csv) output = L"source,target,fstype,options\n" + output;
            else if (opts.outputFormat == OutputFormat::Table) output = L"SOURCE\tTARGET\tFSTYPE\tOPTIONS\n" + output;

            if (!opts.pipeCommand.empty()) {
                ScopedWpOpen pipe(_wpopen(opts.pipeCommand.c_str(), L"w"));
                if (pipe.IsValid()) {
                    std::string narrow = StringHelper::ToUtf8(output);
                    fwrite(narrow.data(), 1, narrow.size(), pipe.Get());
                }
            } else {
                std::wcout << output;
            }
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class FindmntApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        FindmntOptions opts;
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"findmnt";

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

        auto entries = MountInspector::EnumerateMounts();
        FindmntReporter::Report(entries, opts);

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    FindmntApplication app;
    return app.Run(argc, argv);
}
