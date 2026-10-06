#pragma once

#include "uniq.hpp"

class LineReader {
private:
    std::istream& stream;
    char delimiter;

public:
    LineReader(std::istream& in, bool zeroTerminated);

    bool readNextLine(std::string& line);
};
