#pragma once

#include "printf.hpp"
#include "options.hpp"
#include "engine.hpp"

class PrintfApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};
