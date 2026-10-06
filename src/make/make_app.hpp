#ifndef MAKE_APP_HPP
#define MAKE_APP_HPP

#include "options.hpp"

class MakeApp {
private:
    Config cfg;

public:
    explicit MakeApp(Config config);
    int run();
};

#endif // MAKE_APP_HPP
