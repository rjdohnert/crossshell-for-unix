#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <cstdlib>
#include <cwctype>
#include <cstdio>
#include <streambuf>
#include <memory>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

enum class OutputFormat {
    Text = 0,
    Json = 1,
    Csv = 2,
    Tsv = 3,
    Table = 4
};

enum class LimitMode {
    Both = 0,
    Soft = 1,
    Hard = 2
};
