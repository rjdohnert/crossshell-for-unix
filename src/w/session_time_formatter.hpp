#pragma once

#include "string_encoding.hpp"
#include "w.hpp"

std::string FormatLoginTime(ULONGLONG timestamp);

// Format Idle duration (e.g. "0s", "2m", "1:15", "2days")
std::string FormatIdleTime(ULONGLONG idleMs);

// Format CPU time (e.g. "0.12s", "1:05m")
std::string FormatCpuTime(ULONGLONG cpu100ns);

// Calculate CPU Usage percentage
