#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "od.hpp"
#include "options.hpp"
#include "reporter.hpp"

class OdFormatter {
private:
    static const char* ASCII_NAMES[128];

public:
    static void printAddress(std::ostream& out, uint64_t addr, char radix);
    static void formatChunk(std::ostream& out, const uint8_t* data, size_t size, const FormatSpec& fmt);
};

class OdEngine {
private:
    OdOptions options;

public:
    explicit OdEngine(OdOptions opts);
    int execute();
};

#endif // ENGINE_HPP
