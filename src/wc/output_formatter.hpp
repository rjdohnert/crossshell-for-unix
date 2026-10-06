#pragma once

#include "counters.hpp"
#include "wc_options.hpp"
#include "wc.hpp"

class OutputFormatter {
public:
    static void PrintHeader(OutputFormat format);

    static void PrintCounts(const Counters& c, const WcOptions& opts, const std::string& name);
};
