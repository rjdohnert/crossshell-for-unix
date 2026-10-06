#pragma once

#include "stat.hpp"

struct FileSummaryReport {
    std::wstring filePath;
    std::wstring fileName;
    std::wstring fileType;
    std::wstring owner;
    std::wstring creationTime;
    std::wstring lastModifiedTime;
    uint64_t byteSize = 0;
    int64_t lineCount = -1; // -1 if binary or not applicable
    bool isText = false;
    bool isAccessible = false;
    std::wstring errorMessage;
};
