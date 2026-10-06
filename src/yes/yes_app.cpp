#include "yes_app.hpp"
#include "yes_engine.hpp"
#include "yes_options.hpp"

int YesApp::run(int argc, char* argv[]) {
        YesOptions options;
        if (!YesOptions::parse(argc, argv, options)) {
            return 1;
        }
        return YesEngine::execute(options.line);
    }
