#pragma once

#include "counters.hpp"
#include "wc.hpp"

class TextMetricsScanner {
public:
    static void ScanStream(std::istream& in, Counters& c);
};
