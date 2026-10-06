#pragma once

#include "touch.hpp"

class TimeParser {
public:
    static bool IsDigits(const std::wstring& s);

    static bool ParseTimeSpec(const std::wstring& t_str, FILETIME& out_ft);

    static bool GetReferenceTimes(const std::wstring& ref_path, FILETIME& out_at, FILETIME& out_mt);
};
