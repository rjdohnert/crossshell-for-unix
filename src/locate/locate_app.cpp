#include "locate_app.hpp"
#include "engine.hpp"

LocateApp::LocateApp(LocateOptions opts) : options(std::move(opts)) {}

int LocateApp::run() {
    if (options.show_help) {
        CommandLineParser::printUsage();
        return 0;
    }
    if (options.show_version) {
        CommandLineParser::printVersion();
        return 0;
    }
    return LocateEngine::execute(options);
}
