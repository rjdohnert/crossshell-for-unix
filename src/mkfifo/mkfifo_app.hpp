#ifndef MKFIFO_APP_HPP
#define MKFIFO_APP_HPP

#include "mkfifo.hpp"
#include "options.hpp"
#include "engine.hpp"

class MkfifoApp {
public:
    static int run(const FifoOptions& opts);
};

#endif // MKFIFO_APP_HPP
