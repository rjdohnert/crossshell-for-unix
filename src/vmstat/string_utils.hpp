#pragma once

#include "vmstat.hpp"

class StringUtils {
public:
    static std::string WStrToStr(const std::wstring& wstr);

    static bool TryParseInt(const std::string& text, int minValue, int& outValue);

    static std::string FormatLongOrNA(bool available, long value, int width);

    static std::string FormatValueRaw(uint64_t bytes, UnitMode mode);

    static std::string FormatValue(uint64_t bytes, UnitMode mode, int width);
};
