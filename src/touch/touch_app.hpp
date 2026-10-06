#pragma once

#include "file_toucher.hpp"
#include "option_parser.hpp"
#include "touch.hpp"

class TouchApplication {
private:
    OptionParser m_parser;
    FileToucher m_toucher;

public:
    int Run(int argc, wchar_t* argv[]);
};
