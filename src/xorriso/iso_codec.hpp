#pragma once

#include "xorriso.hpp"

class IsoCodec {
public:
    static void PadCopy(char* dest, const std::string& src, size_t maxLen);
};
