#pragma once

#include "session_time_formatter.hpp"
#include "w.hpp"

std::string WStrToStr(const std::wstring& wstr);

// Convert FILETIME to local time string ("10:15am" or "Oct14")
