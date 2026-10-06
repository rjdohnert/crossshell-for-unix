#pragma once

#include "umask.hpp"

class UmaskParser {
public:
    static unsigned int ParseSymbolicMask(unsigned int current_mask, const std::string& expr);

    static unsigned int ParseMaskInput(unsigned int current_mask, const std::string& input);
};
