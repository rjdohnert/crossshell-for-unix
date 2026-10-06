#ifndef PAX_APP_HPP
#define PAX_APP_HPP

#include "pax.hpp"
#include "options.hpp"
#include "engine.hpp"

class PaxApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};

#endif // PAX_APP_HPP
