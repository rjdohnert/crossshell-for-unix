#pragma once

#include "sync_options.hpp"
#include "sync.hpp"

class CliParser {
public:
    static bool parse(int argc, char* argv[], CliOptions& opts);
};
