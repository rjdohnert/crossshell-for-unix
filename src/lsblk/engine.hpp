#pragma once

#include "lsblk.hpp"
#include <string>
#include <vector>

class StringHelper {
public:
    static std::string Utf8Blk(const std::wstring& value);
    static std::string CsvBlk(const std::string& value);
    static std::string JsonBlk(const std::string& value);
    static std::wstring ToLowerCopy(std::wstring value);
    static bool ContainsICase(const std::wstring& haystack, const std::wstring& needle);
    static std::wstring HumanSize(ULONGLONG bytes);
};

class BlockDeviceInspector {
public:
    static std::wstring HealthForDrive(const std::wstring& driveRoot);
    static std::wstring RemoteShareForDrive(const std::wstring& driveRoot);
    static BlockDeviceRow BuildRow(const std::wstring& driveRoot);
    static std::vector<BlockDeviceRow> EnumerateRows();
};
