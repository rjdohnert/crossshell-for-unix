#include "strings_app.hpp"
#include "strings_engine.hpp"
#include "strings_options.hpp"

int StringsApp::run(int argc, char* argv[]) {
        StringsOptions options;
        if (!StringsOptions::parse(argc, argv, options)) {
            return 1;
        }
        StringsEngine engine(std::move(options));
        return engine.execute();
    }
