#pragma once

#ifndef UNICODE
#ifndef UNICODE
#define UNICODE
#endif
#endif
#ifndef _UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#endif
#ifndef NOMINMAX
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "Advapi32.lib")

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstdint>

namespace fs = std::filesystem;


enum class OutputFormat {
    Auto,       // Detailed card if 1 file, Table if multiple
    Table,      // Force tabular format
    Detailed,   // Force detailed card format
    JSON,       // JSON output for scripting pipelines
    CSV         // CSV format for pipeline processing
};
