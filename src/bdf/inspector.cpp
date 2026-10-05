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

#include "inspector.hpp"
#include <cwchar>

std::optional<VolumeMetrics> VolumeInspector::queryPath(const std::wstring& rawPath) {
#ifdef _WIN32
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
#else
    (void)rawPath;
    return std::nullopt;
#endif
}

std::vector<VolumeMetrics> VolumeInspector::enumerateAllVolumes() {
    std::vector<VolumeMetrics> volumes;
#ifdef _WIN32
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
#endif
    return volumes;
}

std::wstring VolumeInspector::getDriveTypeString(UINT driveType) {
#ifdef _WIN32
    switch (driveType) {
        case DRIVE_FIXED:     return L"Local Disk";
        case DRIVE_REMOVABLE: return L"Removable";
        case DRIVE_REMOTE:    return L"Network";
        case DRIVE_CDROM:     return L"CD-ROM";
        case DRIVE_RAMDISK:   return L"RAM Disk";
        default:              return L"Unknown";
    }
#else
    (void)driveType;
    return L"Unknown";
#endif
}
