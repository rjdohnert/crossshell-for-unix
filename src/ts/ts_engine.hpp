#pragma once

#include "timestamp_formatter.hpp"
#include "ts_options.hpp"
#include "ts.hpp"

class TsEngine {
private:
    TsOptions options;
    TimestampFormatter formatter;

public:
    explicit TsEngine(TsOptions opts);

    int execute();
};
