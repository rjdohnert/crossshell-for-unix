#pragma once

#include "diskinfo.hpp"
#include <string>
#include <vector>

class StringHelper {
public:
    static std::wstring CsvBlk(const std::wstring& value);
    static std::wstring JsonBlk(const std::wstring& value);
    static std::wstring ToLowerCopy(std::wstring value);
    static bool ContainsICase(const std::wstring& haystack, const std::wstring& needle);
    static std::wstring HumanSize(ULONGLONG bytes);
};

class BlockDeviceInspector {
public:
    static std::wstring RemoteShareForDrive(const std::wstring& driveRoot);
    static BlockDeviceRow BuildRow(const std::wstring& driveRoot);
    static std::vector<BlockDeviceRow> EnumerateRows();
};
