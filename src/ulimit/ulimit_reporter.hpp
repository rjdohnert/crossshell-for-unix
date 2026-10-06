#pragma once

#include "ulimit.hpp"

class UlimitReporter {
public:
    static void PrintSummary(OutputFormat format);

    static void PrintHelp();

    static void PrintVersion();
};
