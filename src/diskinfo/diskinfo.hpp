#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <winnetwk.h>
#include <string>
#include <vector>

struct BlockDeviceRow {
    std::wstring name;
    std::wstring type;
    std::wstring parent;
    std::wstring fsType;
    std::wstring mountPoint;
    std::wstring size;
    ULONGLONG sizeBytes{0};
    bool sizeKnown{false};
    std::wstring readOnly;
};

enum class OutputFormat {
    Table,
    Csv,
    Json
};
