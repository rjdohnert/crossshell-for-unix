#ifndef NICE_APP_HPP
#define NICE_APP_HPP

#include "nice.hpp"
#include "options.hpp"
#include "engine.hpp"

class NiceApplication {
public:
    int Run(int argc, wchar_t* argv[]) const;
};

#endif // NICE_APP_HPP
