#include "whence_app.hpp"
#include "whence_engine.hpp"
#include "whence_options.hpp"

int WhenceApp::run(int argc, wchar_t* argv[]) {
        WhenceOptions options;
        if (!WhenceOptions::parse(argc, argv, options)) {
            return 1;
        }
        WhenceEngine engine(std::move(options));
        return engine.execute();
    }
