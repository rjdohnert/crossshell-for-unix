#pragma once

#include "options.hpp"

class LnApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};
