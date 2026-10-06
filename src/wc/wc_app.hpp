#pragma once

#include "option_parser.hpp"
#include "wc.hpp"

class WcApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};
