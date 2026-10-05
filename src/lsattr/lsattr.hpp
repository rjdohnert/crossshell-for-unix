#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

struct AttrRow {
    std::wstring flags;
    std::wstring path;
};

enum class OutputFormat {
    Table,
    Csv,
    Json
};
