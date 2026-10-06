#ifndef PATCH_APP_HPP
#define PATCH_APP_HPP

#include "patch.hpp"
#include "options.hpp"
#include "engine.hpp"

class PatchApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]);
};

#endif // PATCH_APP_HPP
