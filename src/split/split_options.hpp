#pragma once

#include "split.hpp"

class SplitOptions {
public:
    SplitMode mode{SplitMode::Lines};
    uint64_t lineCount{1000};
    uint64_t byteCount{0};
    uint64_t chunkCount{0};

    int suffixLen{2};
    SuffixType suffixType{SuffixType::Alpha};

    std::string inputFile{"-"};
    std::string prefix{"x"};
    std::string filterCommand;
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printUsage(const char* progName);

    static uint64_t parseSize(const std::string& str);

    static bool parse(int argc, char* argv[], SplitOptions& opts);
};
