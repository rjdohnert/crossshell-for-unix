#ifndef PGREP_APP_HPP
#define PGREP_APP_HPP

#include "pgrep.hpp"
#include "options.hpp"
#include "engine.hpp"

class PgrepApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};

#endif // PGREP_APP_HPP
