#ifndef NOHUP_APP_HPP
#define NOHUP_APP_HPP

#include "nohup.hpp"
#include "options.hpp"
#include "engine.hpp"

class NohupApplication {
public:
    int Run(int argc, wchar_t* argv[]) const;
};

#endif // NOHUP_APP_HPP
