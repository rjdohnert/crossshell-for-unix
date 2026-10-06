#pragma once

#include "option_parser.hpp"
#include "wipe.hpp"

class WipeApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};
