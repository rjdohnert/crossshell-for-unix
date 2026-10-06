#pragma once

#include "option_parser.hpp"
#include "tail.hpp"

class TailApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const;
};
