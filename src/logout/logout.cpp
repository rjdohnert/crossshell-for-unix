/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * CrossShell for UNIX
 */

#include "logout_app.hpp"

#pragma comment(lib, "user32.lib")

int main(int argc, char* argv[]) {
    LogoutApplication app;
    return app.Run(argc, argv);
}
