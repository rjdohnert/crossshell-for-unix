#pragma once

#include "xargs_options.hpp"
#include "xargs.hpp"

class InputTokenizer {
public:
    static std::vector<std::string> readAll(std::istream& in, const XargsOptions& opts);
};
