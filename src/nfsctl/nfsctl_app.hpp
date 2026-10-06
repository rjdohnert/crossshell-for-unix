#ifndef NFSCTL_APP_HPP
#define NFSCTL_APP_HPP

#include "nfsctl.hpp"
#include "reporter.hpp"
#include "engine.hpp"
#include "controllers.hpp"
#include "options.hpp"

class NfsctlApplication {
public:
    int Run(int argc, wchar_t* argv[]);
};

#endif // NFSCTL_APP_HPP
