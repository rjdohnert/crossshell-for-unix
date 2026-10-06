#pragma once

#include "w.hpp"

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    ULONGLONG cpuTime; // 100-ns units
};
