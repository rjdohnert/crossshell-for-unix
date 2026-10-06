#pragma once

#include "mod_options.hpp"
#include "usermod.hpp"

class InputPipeline {
public:
    static bool IsPipeActive();

    static int ProcessBatchPipe(const ModOptions& baseOpt);
};
