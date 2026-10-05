/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "logname_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include <iostream>

int LognameApplication::Run(int argc, char* argv[]) {
    LognameOptions opts;
    if (!opts.Parse(argc, argv)) {
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage(argc > 0 ? argv[0] : "logname");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    std::string logname = LoginNameProvider::GetLoginName();
    if (!logname.empty()) {
        std::cout << logname << "\n";
        return 0;
    }

    std::cerr << "logname: failed to get effective user name\n";
    return 1;
}
