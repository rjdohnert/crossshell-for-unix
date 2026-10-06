#pragma once

#include "ulimit_options.hpp"
#include "ulimit.hpp"

class JobLimitManager {
public:
    static bool SetJobLimits(HANDLE job_handle, const UlimitOptions& options);

    static bool RunChildUnderLimits(const UlimitOptions& options);
};
