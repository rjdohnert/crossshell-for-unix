#pragma once

#include "threads.hpp"

struct ProcessThreadInfo {
    DWORD pid = 0;
    DWORD threadCount = 0;
    std::wstring name;

    // Converts wide process name into a UTF-8 standard string
    std::string narrowName() const;
};
