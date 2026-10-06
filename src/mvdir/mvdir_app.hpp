#ifndef MVDIR_APP_HPP
#define MVDIR_APP_HPP

#include "mvdir.hpp"
#include "options.hpp"
#include "engine.hpp"

class MvdirApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]);
};

#endif // MVDIR_APP_HPP
