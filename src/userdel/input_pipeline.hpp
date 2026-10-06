#pragma once

#include "delete_options.hpp"
#include "userdel.hpp"

class InputPipeline {
public:
    static bool IsPipeActive();

    static int ProcessBatchPipe(const DeleteOptions& baseOpt);
};
