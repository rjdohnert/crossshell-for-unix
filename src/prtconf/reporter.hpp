#pragma once

#include "prtconf.hpp"
#include "options.hpp"

class PrtconfReporter {
public:
    static void report(const SystemConfigSummary& summary, bool showDevices);
};
