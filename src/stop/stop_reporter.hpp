#pragma once

#include "stop.hpp"

class StopReporter {
public:
    static void Report(OutputFormat format, const std::string& pipeCommand, size_t count, bool hasErrors);
};
