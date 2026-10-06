#pragma once

#include "stat.hpp"

class PipelineManager {
public:
    static bool isInputPiped();

    static std::vector<std::wstring> readPipedPaths();
};
