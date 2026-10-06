#pragma once

#include "xorriso.hpp"

struct FileEntry {
    fs::path relativePath;
    fs::path fullPath;
    UINT64 fileSize = 0;
    UINT32 startLBA = 0;
    bool isDir = false;
};
