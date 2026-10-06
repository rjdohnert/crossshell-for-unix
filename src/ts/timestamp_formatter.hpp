#pragma once

#include "ts_options.hpp"
#include "ts.hpp"

class TimestampFormatter {
private:
    TsOptions options;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point lastTime;

public:
    explicit TimestampFormatter(TsOptions opts);

    std::string generatePrefix();

private:
    static std::string formatSubseconds(int64_t nanos, int precision);

    std::string formatDuration(int64_t totalNanos) const;
};
