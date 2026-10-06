#pragma once

#include "option_parser.hpp"
#include "stopsrc.hpp"

class StopsrcApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};
