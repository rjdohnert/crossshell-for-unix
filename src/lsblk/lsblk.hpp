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
    std::wstring fsType;
    std::wstring mountPoint;
    std::wstring size;
    std::wstring readOnly;
    std::wstring health;
};

enum class OutputFormat {
    Table,
    Csv,
    Json
};
