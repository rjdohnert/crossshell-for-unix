#include "line_reader.hpp"

LineReader::LineReader(std::istream& in, bool zeroTerminated)
        : stream(in), delimiter(zeroTerminated ? '\0' : '\n') {}

bool LineReader::readNextLine(std::string& line) {
        if (!std::getline(stream, line, delimiter)) {
            return false;
        }
        if (delimiter == '\n' && !line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    }
