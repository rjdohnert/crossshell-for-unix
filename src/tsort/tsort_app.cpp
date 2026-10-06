#include "tsort_app.hpp"
#include "tsort_engine.hpp"
#include "tsort_options.hpp"

int TsortApp::run(int argc, char* argv[]) {
        TsortOptions options;
        if (!TsortOptions::parse(argc, argv, options)) {
            return 1;
        }
        TsortEngine engine(std::move(options));
        return engine.execute();
    }
