#pragma once

#include "ps.hpp"

class ProcessRecord {
public:
    DWORD pid = 0;
    DWORD ppid = 0;
    DWORD threads = 0;
    int priority = 0;
    std::wstring name;
    std::wstring user = L"SYSTEM";
    std::wstring status = L"Running";
    size_t workingSetKB = 0;      // Physical memory / RSS
    size_t virtualSizeKB = 0;     // VSZ / Pagefile usage
    std::wstring startTime = L"-";
    std::wstring cpuTime = L"00:00:00";

    std::wstring getField(const std::wstring& key) const;
};

class ProcessCollector {
public:
    static std::vector<ProcessRecord> collect();

private:
    static void enrichProcessData(ProcessRecord& rec);
    static std::wstring formatFileTime(const FILETIME& ft);
    static std::wstring formatDuration(const FILETIME& kernel, const FILETIME& user);
};
