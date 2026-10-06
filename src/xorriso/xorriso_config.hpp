#pragma once

#include "xorriso.hpp"

enum class EngineMode { Help, Mkisofs, Extract, Inspect };

struct XorrisoConfig {
    EngineMode mode = EngineMode::Help;
    std::wstring isoPath;
    std::wstring sourceDir;
    std::wstring extractDir;
    std::string volumeID = "WIN_XORRISO_DISK";
    bool verbose = false;
    bool directIO = true;
};
