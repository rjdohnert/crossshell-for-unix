#pragma once

#include "uptime.hpp"

class SystemMetricsSampler {
private:
    static unsigned long long fileTimeToULL(const FILETIME& ft);

public:
    static double getCpuUtilization();

    static int getActiveUserCount();
};
