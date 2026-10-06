#pragma once

#include "uniq.hpp"

class LineExtractor {
public:
    static std::string_view extractKey(std::string_view line, size_t skipFields, size_t skipChars, size_t checkChars);
};
