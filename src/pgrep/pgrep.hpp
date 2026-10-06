#ifndef PGREP_HPP
#define PGREP_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>

struct ProcessRecord {
    DWORD pid = 0;
    std::wstring name;
};

class ProcessSnapshot {
public:
    static bool Collect(std::vector<ProcessRecord>& out);
};

#endif // PGREP_HPP
