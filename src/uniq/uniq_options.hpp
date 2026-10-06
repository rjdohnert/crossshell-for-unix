#pragma once

#include "uniq.hpp"

class UniqOptions {
public:
    bool count{false};
    bool repeatedOnly{false};
    bool uniqueOnly{false};
    bool ignoreCase{false};
    bool zeroTerminated{false};

    size_t skipFields{0};
    size_t skipChars{0};
    size_t checkChars{0}; // 0 = unlimited

    AllRepeatedMode allRepeated{AllRepeatedMode::None};
    GroupMode groupMode{GroupMode::None};

    std::string inputPath;
    std::string outputPath;

    static void showHelp();

    static void showVersion();

    static UniqOptions parse(int argc, char* argv[]);
};
