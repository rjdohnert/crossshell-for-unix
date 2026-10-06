#pragma once

#include "output_reporter.hpp"
#include "split_options.hpp"
#include "split.hpp"

class SplitEngine {
private:
    SplitOptions options;
    OutputReporter reporter;

public:
    explicit SplitEngine(SplitOptions opts);

    int execute();

private:
    uint64_t splitByLines(std::istream& in);

    uint64_t splitByBytes(std::istream& in);

    uint64_t splitByChunks(std::istream& in);
};
