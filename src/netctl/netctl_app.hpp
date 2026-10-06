#ifndef NETCTL_APP_HPP
#define NETCTL_APP_HPP

#include "netctl.hpp"
#include "options.hpp"
#include "engine.hpp"

class NetctlApplication {
public:
    int Run(int argc, char* argv[]);
};

#endif // NETCTL_APP_HPP
