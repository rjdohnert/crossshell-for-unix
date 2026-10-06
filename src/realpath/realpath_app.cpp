#include "path_resolver.hpp"
#include "realpath_app.hpp"
#include "realpath_options.hpp"

int RealpathApp::run(int argc, char* argv[]) {
        RealpathOptions options;
        if (!RealpathOptions::parse(argc, argv, options)) {
            return 1;
        }
        return PathResolver::execute(options);
    }
