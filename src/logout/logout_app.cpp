/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "logout_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include <iostream>

int LogoutApplication::Run(int argc, char* argv[]) {
    LogoutOptions opts;
    if (!opts.Parse(argc, argv)) {
        opts.PrintUsage(argc > 0 ? argv[0] : "logout");
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage(argc > 0 ? argv[0] : "logout");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    if (!opts.force) {
        std::cout << "Are you sure you want to log out? (y/n): ";
        char response = 0;
        if (!(std::cin >> response) || (response != 'y' && response != 'Y')) {
            std::cout << "Logout canceled." << std::endl;
            return 0;
        }
    }

    std::cout << "Attempting to log out..." << std::endl;
    if (!SessionManager::TerminateSession(opts.force)) {
        std::cerr << "Could not initiate logout." << std::endl;
        return 1;
    }

    return 0;
}
