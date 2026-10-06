#pragma once

#include "option_parser.hpp"
#include "test.hpp"

class TestApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const;
};
