#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsvc.h>
#include <string>
#include <vector>

struct ServiceRow {
    std::string name;
    DWORD pid = 0;
    std::string status;
};

enum class LssrcFormat { Table, Csv, Json };
