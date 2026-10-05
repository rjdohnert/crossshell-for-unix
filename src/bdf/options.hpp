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

#ifndef BDF_OPTIONS_HPP
#define BDF_OPTIONS_HPP

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
using UINT = unsigned int;
#define DRIVE_UNKNOWN 0
#endif

#include <cstdint>
#include <string>
#include <vector>

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

class CommandLineParser {
public:
    static ProgramOptions parse(int argc, wchar_t* argv[]);

private:
    static uint64_t parseBlockSize(const std::wstring& val);
};

#endif // BDF_OPTIONS_HPP
