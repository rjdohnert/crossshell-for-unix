/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 */

#include "spawn.hpp"
#include "spawn_options.hpp"
#include "spawn_engine.hpp"

class SpawnApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        SpawnOptions options;
        if (!SpawnOptions::parse(argc, argv, options)) {
            return 1;
        }

        SpawnEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return SpawnApp::run(argc, argv);
}