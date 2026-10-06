#include "split_app.hpp"
#include "split_engine.hpp"
#include "split_options.hpp"

int SplitApp::run(int argc, char* argv[]) {
        SplitOptions options;
        if (!SplitOptions::parse(argc, argv, options)) {
            return 1;
        }

        SplitEngine engine(std::move(options));
        return engine.execute();
    }
