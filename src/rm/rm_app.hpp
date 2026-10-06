#pragma once

#include "option_parser.hpp"
#include "rm.hpp"

class RmApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};
