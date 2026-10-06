#pragma once

#include "useradd.hpp"

class InputPipeline {
public:
    static bool IsPipeActive();

    static void ProcessBatchPipe(bool verbose);
};
