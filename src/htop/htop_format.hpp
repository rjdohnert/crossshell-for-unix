#pragma once

#include "htop_types.hpp"

std::wstring FormatBytes(SIZE_T bytes);
std::wstring FormatCpuTime(ULONGLONG time100ns);
std::wstring FormatUptime(ULONGLONG seconds);
