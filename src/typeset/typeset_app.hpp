#pragma once

#include "option_parser.hpp"
#include "typeset.hpp"

class TypesetApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};
