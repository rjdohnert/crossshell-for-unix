#pragma once

#include "xargs.hpp"

class ProcessBatchRunner {
public:
    static int runCommand(const std::vector<std::string>& cmdArgs, bool verbose);
};
