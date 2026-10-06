#pragma once

#include "xorriso_config.hpp"
#include "xorriso.hpp"

class IsoInspector {
public:
    static bool Inspect(const XorrisoConfig& cfg);
};
