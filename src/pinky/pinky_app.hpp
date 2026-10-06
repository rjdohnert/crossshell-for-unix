#ifndef PINKY_APP_HPP
#define PINKY_APP_HPP

#include "pinky.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class PinkyApplication {
public:
    int Run(int argc, wchar_t* argv[]);
};

#endif // PINKY_APP_HPP
