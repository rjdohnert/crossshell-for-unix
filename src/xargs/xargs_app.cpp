#include "xargs_app.hpp"
#include "xargs_engine.hpp"
#include "xargs_options.hpp"

int XargsApp::run(int argc, char* argv[]) {
        XargsOptions options;
        if (!XargsOptions::parse(argc, argv, options)) {
            return 1;
        }
        XargsEngine engine(std::move(options));
        return engine.execute();
    }
