#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class PatternParser {
public:
    static bool parse(std::string_view input, std::vector<uint8_t>& bytes,
                      std::string& error);
};
