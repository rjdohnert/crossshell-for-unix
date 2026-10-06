/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "sha256sum.hpp"
#include "sha256_options.hpp"
#include "sha256_engine.hpp"

class Sha256App {
public:
    static int run(int argc, char* argv[]) {
        Sha256Options options;
        if (!Sha256Options::parse(argc, argv, options)) {
            return 1;
        }
        Sha256Engine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return Sha256App::run(argc, argv);
}
