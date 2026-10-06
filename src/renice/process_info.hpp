#pragma once

#include "renice.hpp"

struct ProcessInfo {
    DWORD pid{0};
    std::wstring name;
    std::wstring owner;
    DWORD currentPriorityClass{0};
};
