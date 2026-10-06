#ifndef PASTE_APP_HPP
#define PASTE_APP_HPP

#include "paste.hpp"
#include "options.hpp"
#include "engine.hpp"

class PasteApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};

#endif // PASTE_APP_HPP
