#pragma once

#include "ts.hpp"

enum class TsMode {
    WallClock,           // Standard calendar timestamp
    ElapsedSinceStart,   // -s: Time elapsed since ts was started
    Incremental          // -i: Time elapsed since the previous line
};

class TsOptions {
public:
    static constexpr std::string_view VERSION = "1.2.0";

    TsMode mode{TsMode::WallClock};
    bool useUtc{false};               // -u
    bool isIso{false};                // -z, --iso
    int subsecondPrecision{-1};       // -1: default, 0: none, 3: ms, 6: us, 9: ns
    std::string customFormat;         // Custom strftime format
    bool flushEveryLine{true};        // Real-time line flushing
    bool timestampEmptyLines{true};
    int outputFormat{0};              // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printVersion();

    static void printHelp(const char* exeName);

    static bool parse(int argc, char* argv[], TsOptions& opts);
};
