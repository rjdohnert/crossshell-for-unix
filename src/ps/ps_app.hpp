#pragma once

#include "ps.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class PsApplication {
public:
    static void displayHelp();
    static int run(int argc, wchar_t* argv[]);
};
