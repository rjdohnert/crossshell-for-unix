#include "which_app.hpp"
#include "which_engine.hpp"
#include "which_options.hpp"

int WhichApp::run(int argc, char* argv[]) {
        WhichOptions options;
        if (!WhichOptions::parse(argc, argv, options)) {
            return 1;
        }
        WhichEngine engine(std::move(options));
        return engine.execute();
    }
