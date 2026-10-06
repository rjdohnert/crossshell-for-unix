#pragma once

#include "xorriso_config.hpp"
#include "xorriso.hpp"

class IsoExtractor {
public:
    static bool ExtractDirectory(HANDLE hIso, UINT32 lba, UINT32 length, const fs::path& targetDir);

    static bool Extract(const XorrisoConfig& cfg);
};
