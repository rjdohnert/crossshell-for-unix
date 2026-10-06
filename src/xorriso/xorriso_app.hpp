#pragma once

#include "option_parser.hpp"
#include "xorriso.hpp"

class XorrisoApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};
