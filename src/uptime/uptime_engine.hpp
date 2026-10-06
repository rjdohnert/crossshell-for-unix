#pragma once

#include "uptime_options.hpp"
#include "uptime.hpp"

class UptimeEngine {
private:
    UptimeOptions options;

public:
    explicit UptimeEngine(UptimeOptions opts);

    int execute();
};
