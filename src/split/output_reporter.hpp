#pragma once

#include "split.hpp"

class OutputReporter {
private:
    int format;
    std::string pipeCommand;

public:
    OutputReporter(int fmt, std::string pipeCmd);

    int finish(const std::string& prefix, uint64_t files) const;
};
