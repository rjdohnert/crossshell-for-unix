#pragma once

#include "tr.hpp"

class PatternExpander {
public:
    static std::vector<uint8_t> expand(const std::string& str, size_t targetLen = 0);
};
