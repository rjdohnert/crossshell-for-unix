#ifndef TELEMETRY_REPORTER_HPP
#define TELEMETRY_REPORTER_HPP

#include "sleep.hpp"

class TelemetryReporter {
public:
    static void report(const SleepOptions& options, long long msToSleep, double totalSleepMs);
};

#endif // TELEMETRY_REPORTER_HPP
