#pragma once

#include "ptime.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class PtimeApplication {
public:
    int Run(int argc, wchar_t* argv[]) const;
};
