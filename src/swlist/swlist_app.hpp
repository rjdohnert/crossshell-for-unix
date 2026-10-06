#pragma once

#include "option_parser.hpp"
#include "swlist.hpp"

class SwlistApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const;
};
