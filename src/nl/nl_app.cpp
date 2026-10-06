#include "nl_app.hpp"

int NlApp::run(int argc, char* argv[]) {
    NlOptions options;
    if (!NlOptions::parse(argc, argv, options)) {
        return 1;
    }
    NlEngine engine(std::move(options));
    return engine.execute();
}
