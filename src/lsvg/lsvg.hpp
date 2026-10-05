#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <string>
#include <vector>
#include <cstdint>

// Constants for (Physical Partition) (Standard 64MB default)
constexpr UINT64 PP_SIZE_MB = 64;
constexpr UINT64 PP_SIZE_BYTES = PP_SIZE_MB * 1024 * 1024;

struct LogicalVolume {
    std::string name;          // e.g., hd4, hd2, lv_data
    std::string type;          // e.g., ntfs, refs, raw
    UINT32 lpCount = 0;        // Logical Partitions
    UINT32 ppCount = 0;        // Physical Partitions
    UINT32 pvCount = 0;        // Physical Volumes spanned
    std::string state;         // open/syncd, closed/syncd
    std::string mountPoint;    // e.g., C: and D:
};

struct PhysicalVolume {
    std::string name;          // e.g., PhysicalDrive0
    UINT32 deviceNumber = 0;
    std::string state = "active";
    UINT64 totalSizeBytes = 0;
    UINT32 totalPPs = 0;
    UINT32 freePPs = 0;
};

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

enum class LsvgFormat { Table, Csv, Json };
