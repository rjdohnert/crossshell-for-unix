#ifndef PBCOPY_APP_HPP
#define PBCOPY_APP_HPP

#include "pbcopy.hpp"
#include "options.hpp"
#include "engine.hpp"

class PbcopyApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};

#endif // PBCOPY_APP_HPP
