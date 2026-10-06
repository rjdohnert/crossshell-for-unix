/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "seq.hpp"
#include "seq_options.hpp"
#include "sequence_generator.hpp"

class SeqApp {
public:
    static int run(int argc, char* argv[]) {
        SeqOptions options;
        if (!SeqOptions::parse(argc, argv, options)) {
            return 1;
        }

        SeqEngine engine(std::move(options));
        return engine.execute(std::cout);
    }
};

int main(int argc, char* argv[]) {
    return SeqApp::run(argc, argv);
}
