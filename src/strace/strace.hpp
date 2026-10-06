#pragma once

#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <streambuf>
#include <memory>

#pragma comment(lib, "dbghelp.lib")

constexpr DWORD STATUS_WX86_BREAKPOINT_VALUE = 0x4000001F;

enum class OutputFormat {
    Human,
    Json,
    Csv,
    Table
};

struct SummaryStats {
    DWORD calls = 0;
    double seconds = 0.0;
};
