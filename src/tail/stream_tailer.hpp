#pragma once

#include "tail.hpp"

class StreamTailer {
public:
    static bool TailLines(std::istream& in, long long count, bool from_start);

    static bool TailBytes(std::istream& in, long long count, bool from_start);
};
