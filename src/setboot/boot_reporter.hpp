#ifndef BOOT_REPORTER_HPP
#define BOOT_REPORTER_HPP

#include "setboot.hpp"

class SetbootReporter {
public:
    static void display(const BootEnvironment& env, bool verbose);
};

#endif // BOOT_REPORTER_HPP
