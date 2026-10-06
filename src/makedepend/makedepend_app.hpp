#ifndef MAKEDEPEND_APP_HPP
#define MAKEDEPEND_APP_HPP

#include "options.hpp"
#include "engine.hpp"

class MakedependApp {
public:
    static int run(const MakedependOptions& opts);
};

#endif // MAKEDEPEND_APP_HPP
