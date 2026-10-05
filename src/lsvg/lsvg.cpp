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
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <cstdint>

static std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string output(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &output[0], required, nullptr, nullptr);
    return output;
}

// Constants for (Physical Partition) (Standard 64MB default)
constexpr UINT64 PP_SIZE_MB = 64;
constexpr UINT64 PP_SIZE_BYTES = PP_SIZE_MB * 1024 * 1024;

// RAII Handle Wrapper for Kernel Device Objects
class Win32Handle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    Win32Handle(HANDLE h) : h_(h) {}
    ~Win32Handle() { if (isValid()) CloseHandle(h_); }
    bool isValid() const { return h_ != INVALID_HANDLE_VALUE && h_ != nullptr; }
    operator HANDLE() const { return h_; }
};

// Logical Volume (LV) Representation
struct LogicalVolume {
    std::string name;          // e.g., hd4, hd2, lv_data
    std::string type;          // e.g., ntfs, refs, raw
    UINT32 lpCount = 0;        // Logical Partitions
    UINT32 ppCount = 0;        // Physical Partitions
    UINT32 pvCount = 0;        // Physical Volumes spanned
    std::string state;         // open/syncd, closed/syncd
    std::string mountPoint;    // e.g., C: and D:
};

// Physical Volume (PV) Representation
struct PhysicalVolume {
    std::string name;          // e.g., PhysicalDrive0
    UINT32 deviceNumber = 0;
    std::string state = "active";
    UINT64 totalSizeBytes = 0;
    UINT32 totalPPs = 0;
    UINT32 freePPs = 0;
};

// Volume Group (VG) Representation
struct VolumeGroup {
    std::string name;          // e.g., rootvg, datavg
    std::string vgIdentifier;
    std::string state = "active";
    std::string permission = "read/write";
    UINT64 totalSizeBytes = 0;
    UINT64 freeSizeBytes = 0;
    UINT32 ppSizeMB = PP_SIZE_MB;
    UINT32 maxLVs = 256;
    UINT32 quorum = 2;
    std::vector<PhysicalVolume> pvs;
    std::vector<LogicalVolume> lvs;
};

// CLI Command Options
struct CmdOptions {
    bool listActiveOnly = false; // -o
    bool listLVs = false;        // -l
    bool listPVs = false;        // -p
    bool listAllDetail = false;  // -a
    std::string targetVG;
    enum class Format { Table, Csv, Json } format = Format::Table;
    std::vector<std::string> filters;
};

static std::string csv_vg(const std::string& value) { std::string out="\""; for(char c:value)out+=c=='"'?"\"\"":std::string(1,c); return out+'"'; }
static std::string json_vg(const std::string& value) { std::string out; for(char c:value){if(c=='"'||c=='\\')out+='\\';if(c=='\n')out+="\\n";else out+=c;}return out; }

// Query System Volume Groups via NT Kernel IOCTLs
class StorageManager {
public:
    static std::vector<VolumeGroup> DiscoverVolumeGroups() {
        std::vector<VolumeGroup> vgs;

        // Group 1: 'rootvg' (System Disk & Operating System Partitions)
        VolumeGroup rootvg;
        rootvg.name = "rootvg";
        rootvg.vgIdentifier = "00f6123a00004c00";

        // Query PhysicalDrive0 (Primary System Drive)
        PhysicalVolume pv0;
        pv0.name = "hdisk0";
        pv0.deviceNumber = 0;

        Win32Handle hDrive = CreateFileW(
            L"\\\\.\\PhysicalDrive0",
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (hDrive.isValid()) {
            GET_LENGTH_INFORMATION lengthInfo{};
            DWORD bytesReturned = 0;
            if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, nullptr)) {
                pv0.totalSizeBytes = lengthInfo.Length.QuadPart;
            } else {
                pv0.totalSizeBytes = 512ULL * 1024 * 1024 * 1024;
            }

            pv0.totalPPs = static_cast<UINT32>(pv0.totalSizeBytes / PP_SIZE_BYTES);
            pv0.freePPs = static_cast<UINT32>(pv0.totalPPs * 0.25);
            rootvg.pvs.push_back(pv0);

            wchar_t driveBuffer[4096] = {};
            DWORD driveChars = GetLogicalDriveStringsW(ARRAYSIZE(driveBuffer), driveBuffer);
            wchar_t* pDrive = driveBuffer;
            while (driveChars > 0 && *pDrive) {
                LogicalVolume lv;
                lv.name = "lv_" + WideToUtf8(std::wstring(pDrive));
                lv.type = "ntfs";
                lv.lpCount = 1;
                lv.ppCount = 1;
                lv.pvCount = 1;
                lv.state = "open/syncd";
                lv.mountPoint = WideToUtf8(std::wstring(pDrive));
                rootvg.lvs.push_back(lv);
                pDrive += wcslen(pDrive) + 1;
                driveChars = static_cast<DWORD>(driveBuffer + ARRAYSIZE(driveBuffer) - pDrive);
            }
        }

        rootvg.totalSizeBytes = pv0.totalSizeBytes;
        rootvg.freeSizeBytes = static_cast<UINT64>(pv0.freePPs) * PP_SIZE_BYTES;
        vgs.push_back(rootvg);

        Win32Handle hDrive1 = CreateFileW(
            L"\\\\.\\PhysicalDrive1",
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (hDrive1.isValid()) {
            VolumeGroup datavg;
            datavg.name = "datavg";
            datavg.vgIdentifier = "00f6123b77a11e00";

            PhysicalVolume pv1;
            pv1.name = "hdisk1";
            pv1.deviceNumber = 1;

            GET_LENGTH_INFORMATION lengthInfo{};
            DWORD bytesReturned = 0;
            if (DeviceIoControl(hDrive1, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, nullptr)) {
                pv1.totalSizeBytes = lengthInfo.Length.QuadPart;
            } else {
                pv1.totalSizeBytes = 1024ULL * 1024 * 1024 * 1024;
            }

            pv1.totalPPs = static_cast<UINT32>(pv1.totalSizeBytes / PP_SIZE_BYTES);
            pv1.freePPs = static_cast<UINT32>(pv1.totalPPs * 0.40);
            datavg.pvs.push_back(pv1);

            LogicalVolume lvData;
            lvData.name = "lv_data01";
            lvData.type = "ntfs";
            lvData.ppCount = pv1.totalPPs - pv1.freePPs;
            lvData.lpCount = lvData.ppCount;
            lvData.pvCount = 1;
            lvData.state = "open/syncd";
            lvData.mountPoint = "E:\\";
            datavg.lvs.push_back(lvData);

            datavg.totalSizeBytes = pv1.totalSizeBytes;
            datavg.freeSizeBytes = static_cast<UINT64>(pv1.freePPs) * PP_SIZE_BYTES;

            vgs.push_back(datavg);
        }

        return vgs;
    }
};

// Header & Help Menu Output
void PrintHelp() {
    std::cout << R"(lsvg(1)                  CrossShell for UNIX Reference Manual                 lsvg(1)

    NAME
        lsvg - report Windows volume groups

    SYNOPSIS
        lsvg [OPTIONS] [VOLUME_GROUP]
        lsvg -l VOLUME_GROUP
        lsvg -p VOLUME_GROUP
        lsvg -a

    DESCRIPTION
        ReportsWindows storage architectures. Physical
        drives and logical disks are represented as volume groups, physical
        partitions, and logical volumes.

    OPTIONS
        -o
            Select the active-volume-group mode.

        -l VOLUME_GROUP
            Display logical-volume details for a volume group.

        -p VOLUME_GROUP
            Display physical-volume details for a volume group.

        -a
            Display detailed status for all volume groups.

        --table
            Use aligned table output (default).

        --csv
            Emit volume-group summaries as CSV.

        --json
            Emit volume-group summaries as JSON.

        -
            Read volume-group filters from standard input.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        lsvg
            Display the volume-group summary.

        lsvg -o
            Select active volume groups.

        lsvg rootvg
            Display the summary for rootvg.

        lsvg -l rootvg
            Display logical-volume details for rootvg.

        lsvg -p rootvg
            Display physical-volume details for rootvg.

        lsvg -a
            Display detailed summaries for all volume groups.

    EXIT STATUS
        0
            Successful report generation.
        1
            Invalid options or unknown volume group.

    CrossShell for UNIX                                                    lsvg(1)
)";
}

// Display Detailed VG Status (Standard lsvg output)
void PrintVGDetail(const VolumeGroup& vg) {
    UINT32 totalPPs = static_cast<UINT32>(vg.totalSizeBytes / PP_SIZE_BYTES);
    UINT32 freePPs = static_cast<UINT32>(vg.freeSizeBytes / PP_SIZE_BYTES);
    UINT32 usedPPs = totalPPs - freePPs;
    UINT32 openLVs = 0;
    for (const auto& lv : vg.lvs) {
        if (lv.state.find("open") != std::string::npos) openLVs++;
    }

    std::cout << std::left
              << std::setw(20) << "VOLUME GROUP:" << std::setw(25) << vg.name
              << std::setw(16) << "VG IDENTIFIER:" << vg.vgIdentifier << "\n"
              << std::setw(20) << "VG STATE:" << std::setw(25) << vg.state
              << std::setw(16) << "PP SIZE:" << vg.ppSizeMB << " megabyte(s)\n"
              << std::setw(20) << "VG PERMISSION:" << std::setw(25) << vg.permission
              << std::setw(16) << "TOTAL PPs:" << totalPPs << " (" << (totalPPs * vg.ppSizeMB) << " MB)\n"
              << std::setw(20) << "MAX LVs:" << std::setw(25) << vg.maxLVs
              << std::setw(16) << "FREE PPs:" << freePPs << " (" << (freePPs * vg.ppSizeMB) << " MB)\n"
              << std::setw(20) << "LVs:" << std::setw(25) << vg.lvs.size()
              << std::setw(16) << "USED PPs:" << usedPPs << " (" << (usedPPs * vg.ppSizeMB) << " MB)\n"
              << std::setw(20) << "OPEN LVs:" << std::setw(25) << openLVs
              << std::setw(16) << "QUORUM:" << vg.quorum << "\n"
              << std::setw(20) << "TOTAL PVs:" << std::setw(25) << vg.pvs.size()
              << std::setw(16) << "ACTIVE PVs:" << vg.pvs.size() << "\n"
              << std::setw(20) << "STALE PVs:" << std::setw(25) << 0
              << std::setw(16) << "MAX PPs per PV:" << 1016 << "\n\n";
}

// Display Logical Volumes (-l flag)
void PrintLVDetail(const VolumeGroup& vg) {
    std::cout << vg.name << ":\n";
    std::cout << std::left
              << std::setw(18) << "LV NAME"
              << std::setw(12) << "TYPE"
              << std::setw(8)  << "LPs"
              << std::setw(8)  << "PPs"
              << std::setw(6)  << "PVs"
              << std::setw(14) << "LV STATE"
              << "MOUNT POINT\n";
    std::cout << std::string(82, '-') << "\n";

    for (const auto& lv : vg.lvs) {
        std::cout << std::left
                  << std::setw(18) << lv.name
                  << std::setw(12) << lv.type
                  << std::setw(8)  << lv.lpCount
                  << std::setw(8)  << lv.ppCount
                  << std::setw(6)  << lv.pvCount
                  << std::setw(14) << lv.state
                  << lv.mountPoint << "\n";
    }
    std::cout << "\n";
}

// Display Physical Volumes (-p flag)
void PrintPVDetail(const VolumeGroup& vg) {
    std::cout << vg.name << ":\n";
    std::cout << std::left
              << std::setw(18) << "PV_NAME"
              << std::setw(16) << "PV STATE"
              << std::setw(12) << "TOTAL PPs"
              << std::setw(12) << "FREE PPs"
              << "FREE DISTRIBUTION\n";
    std::cout << std::string(82, '-') << "\n";

    for (const auto& pv : vg.pvs) {
        std::ostringstream dist;
        dist << (pv.freePPs / 5) << ".." << (pv.freePPs / 5) << ".." 
             << (pv.freePPs / 5) << ".." << (pv.freePPs / 5) << ".." 
             << (pv.freePPs - (4 * (pv.freePPs / 5)));

        std::cout << std::left
                  << std::setw(18) << pv.name
                  << std::setw(16) << pv.state
                  << std::setw(12) << pv.totalPPs
                  << std::setw(12) << pv.freePPs
                  << dist.str() << "\n";
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    CmdOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "-help" || arg == "/?" || arg == "--help") {
            PrintHelp();
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            std::cout << "lsvg 1.0.0\n";
            return 0;
        } else if (arg == "-o") {
            opts.listActiveOnly = true;
        } else if (arg == "-l") {
            opts.listLVs = true;
        } else if (arg == "-p") {
            opts.listPVs = true;
        } else if (arg == "-a") {
            opts.listAllDetail = true;
        } else if (arg == "--table") {
            opts.format = CmdOptions::Format::Table;
        } else if (arg == "--csv") {
            opts.format = CmdOptions::Format::Csv;
        } else if (arg == "--json") {
            opts.format = CmdOptions::Format::Json;
        } else if (arg == "-") {
            std::string filter; while (std::cin >> filter) opts.filters.push_back(filter);
        } else if (arg[0] != '-') {
            opts.targetVG = arg;
        }
    }

    auto vgs = StorageManager::DiscoverVolumeGroups();
    if (!opts.filters.empty()) opts.targetVG = opts.filters.front();
    if (opts.format != CmdOptions::Format::Table) {
        std::vector<VolumeGroup> selected;
        for (const auto& vg : vgs) {
            bool keep = opts.targetVG.empty();
            if (!opts.targetVG.empty()) keep = _stricmp(vg.name.c_str(), opts.targetVG.c_str()) == 0;
            if (keep) selected.push_back(vg);
        }
        if (opts.format == CmdOptions::Format::Csv) {
            std::cout << "Name,Identifier,State,Permission,TotalBytes,FreeBytes,LogicalVolumes,PhysicalVolumes\n";
            for (const auto& vg : selected) std::cout << csv_vg(vg.name) << ',' << csv_vg(vg.vgIdentifier) << ',' << csv_vg(vg.state) << ',' << csv_vg(vg.permission) << ',' << vg.totalSizeBytes << ',' << vg.freeSizeBytes << ',' << vg.lvs.size() << ',' << vg.pvs.size() << '\n';
        } else {
            std::cout << "[\n";
            for (size_t i=0;i<selected.size();++i) { const auto& vg=selected[i]; std::cout << "  {\"name\":\""<<json_vg(vg.name)<<"\",\"identifier\":\""<<json_vg(vg.vgIdentifier)<<"\",\"state\":\""<<json_vg(vg.state)<<"\",\"permission\":\""<<json_vg(vg.permission)<<"\",\"totalBytes\":"<<vg.totalSizeBytes<<",\"freeBytes\":"<<vg.freeSizeBytes<<",\"logicalVolumes\":"<<vg.lvs.size()<<",\"physicalVolumes\":"<<vg.pvs.size()<<"}"<<(i+1==selected.size()?"\n":",\n"); }
            std::cout << "]\n";
        }
        return selected.empty() ? 1 : 0;
    }

    // Mode 1: Plain 'lsvg' or 'lsvg -o' -> show the detailed summary for each discovered VG
    if (!opts.listAllDetail && !opts.listLVs && !opts.listPVs && opts.targetVG.empty()) {
        for (const auto& vg : vgs) {
            PrintVGDetail(vg);
        }
        return 0;
    }

    // Mode 2: 'lsvg -a' (All VG details)
    if (opts.listAllDetail) {
        for (const auto& vg : vgs) {
            PrintVGDetail(vg);
        }
        return 0;
    }

    // Mode 3: Specific VG requested
    auto it = std::find_if(vgs.begin(), vgs.end(), [&](const VolumeGroup& v) {
        return _stricmp(v.name.c_str(), opts.targetVG.c_str()) == 0;
    });

    if (it == vgs.end()) {
        std::cerr << "0516-010 lsvg: Volume group " << opts.targetVG << " does not exist or is not active.\n";
        return 1;
    }

    const auto& vg = *it;

    if (opts.listLVs) {
        PrintLVDetail(vg);
    } else if (opts.listPVs) {
        PrintPVDetail(vg);
    } else {
        PrintVGDetail(vg);
    }

    return 0;
}