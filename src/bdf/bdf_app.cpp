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
 *
 * CrossShell for UNIX
 */

#include "bdf_app.hpp"
#include "formatter.hpp"
#include "inspector.hpp"
#include "table.hpp"
#include "pipeline.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

DiskFreeApplication::DiskFreeApplication(ProgramOptions options)
    : m_opts(std::move(options)) {}

int DiskFreeApplication::run() {
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

bool DiskFreeApplication::matchesFilter(const VolumeMetrics& vol) const {
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

void DiskFreeApplication::displayMetrics(const std::vector<VolumeMetrics>& volumes) const {
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

bool DiskFreeApplication::equalIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.length() != b.length()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](wchar_t c1, wchar_t c2) {
        return std::towlower(c1) == std::towlower(c2);
    });
}

void DiskFreeApplication::printVersion() {
    std::wcout << L"bdf version 5.1.0\n";
    std::wcout << L"Copyright (C) 2026, Roberto J Dohnert.\n";
}

void DiskFreeApplication::printHelp() {
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

int BdfApp::run(int argc, wchar_t* argv[]) {
#ifdef _WIN32
    bool stdoutConsole = _isatty(_fileno(stdout)) != 0;
    bool stderrConsole = _isatty(_fileno(stderr)) != 0;
    _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stderr), stderrConsole ? _O_U16TEXT : _O_U8TEXT);
#endif

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
