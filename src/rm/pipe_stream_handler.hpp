#pragma once

#include "rm.hpp"

class PipeStreamHandler {
public:
    static std::vector<std::string> ReadTargetsFromStream(bool nullDelimited);
};
