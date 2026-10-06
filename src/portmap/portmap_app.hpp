#ifndef PORTMAP_APP_HPP
#define PORTMAP_APP_HPP

#include "portmap.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class PortmapApplication {
public:
    int Run(int argc, char* argv[]);
};

#endif // PORTMAP_APP_HPP
