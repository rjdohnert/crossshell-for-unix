#ifndef PKILL_APP_HPP
#define PKILL_APP_HPP

#include "pkill.hpp"
#include "options.hpp"
#include "engine.hpp"

class PkillApplication {
public:
    int Run(int argc, wchar_t* argv[]) const;
};

#endif // PKILL_APP_HPP
