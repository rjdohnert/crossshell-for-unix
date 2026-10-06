/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "sleep.hpp"
#include "sleep_options.hpp"
#include "sleep_engine.hpp"

class SleepApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        SleepOptions options;
        if (!SleepOptionsParser::parse(argc, argv, options)) {
            return 1;
        }

        if (options.show_help) {
            SleepOptionsParser::printHelp();
            return 0;
        }

        if (options.show_version) {
            SleepOptionsParser::printVersion();
            return 0;
        }

        return SleepEngine::execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SleepApplication app;
    return app.Run(argc, argv);
}