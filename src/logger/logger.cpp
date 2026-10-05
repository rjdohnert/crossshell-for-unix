/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * CrossShell for UNIX
 */

#include "logger_app.hpp"

#pragma comment(lib, "Advapi32.lib")

int wmain(int argc, wchar_t* argv[]) {
    LoggerApplication app;
    return app.Run(argc, argv);
}
