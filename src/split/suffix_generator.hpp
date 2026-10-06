#pragma once

#include "split.hpp"

class SuffixGenerator {
public:
    static std::string generate(uint64_t index, int len, SuffixType type);
};
