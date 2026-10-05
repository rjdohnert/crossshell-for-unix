/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * CrossShell for UNIX
 */

#include "logname_app.hpp"

#pragma comment(lib, "advapi32.lib")

int main(int argc, char* argv[]) {
    LognameApplication app;
    return app.Run(argc, argv);
}
