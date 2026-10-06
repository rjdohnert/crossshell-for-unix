#pragma once

#include "option_parser.hpp"
#include "recycle.hpp"

class RecycleApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};
