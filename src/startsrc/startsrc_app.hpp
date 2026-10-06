#pragma once

#include "option_parser.hpp"
#include "startsrc.hpp"

class StartsrcApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};
