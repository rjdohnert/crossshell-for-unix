#ifndef LTRACE_APP_HPP
#define LTRACE_APP_HPP

#include "ltrace.hpp"

class LtraceApp {
private:
    Config config;
public:
    explicit LtraceApp(Config cfg);
    int run();
};

#endif // LTRACE_APP_HPP
