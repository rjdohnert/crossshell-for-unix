/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 */

#include "sequence.hpp"
#include "sequence_options.hpp"
#include "sort_pipeline.hpp"

class SequenceApp {
public:
    static int run(int argc, char* argv[]) {
        SequenceOptions options;
        if (!SequenceOptions::parse(argc, argv, options)) {
            return 2;
        }

        SequenceEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return SequenceApp::run(argc, argv);
}