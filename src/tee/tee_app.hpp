#pragma once

#include "option_parser.hpp"
#include "tee.hpp"

class TeeApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const;
};
