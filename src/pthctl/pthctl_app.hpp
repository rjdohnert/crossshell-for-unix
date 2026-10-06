#pragma once

#include "pthctl.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class PthctlApp {
public:
    static int run(int argc, wchar_t* argv[]);
};
