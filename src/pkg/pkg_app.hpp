#ifndef PKG_APP_HPP
#define PKG_APP_HPP

#include "pkg.hpp"
#include "options.hpp"
#include "engine.hpp"

class PkgApp {
public:
    static int run(int argc, wchar_t* argv[]);
};

#endif // PKG_APP_HPP
