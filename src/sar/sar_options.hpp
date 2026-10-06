#ifndef SAR_OPTIONS_HPP
#define SAR_OPTIONS_HPP

#include "sar.hpp"

class SarOptionParser {
public:
    static void printHelp();
    static bool parse(int argc, char* argv[], SarOptions& opts);
};

#endif // SAR_OPTIONS_HPP
