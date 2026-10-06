#pragma once

#include "option_parser.hpp"
#include "suspend.hpp"

class SuspendApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const;
};
