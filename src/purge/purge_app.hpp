#pragma once

#include "purge.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class PurgeApplication {
public:
    int Run(int argc, char* argv[]) const;
};
