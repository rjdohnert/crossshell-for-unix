#pragma once

#include "tsort.hpp"

class TsortReporter {
public:
    static int dispatch(const std::vector<std::string>& nodes, int format, const std::string& pipeCommand);
};
