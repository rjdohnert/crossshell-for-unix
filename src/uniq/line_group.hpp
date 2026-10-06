#pragma once

#include "uniq.hpp"

struct LineGroup {
    std::string representativeLine;
    std::vector<std::string> allLines;
    uint64_t count{0};

    void reset(std::string firstLine);

    void add(std::string line);
};
