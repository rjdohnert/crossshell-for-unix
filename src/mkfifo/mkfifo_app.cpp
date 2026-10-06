#include "mkfifo_app.hpp"

int MkfifoApp::run(const FifoOptions& opts) {
    if (opts.showHelp) {
        HelpFormatter::printHelp();
        return 0;
    }
    if (opts.showVersion) {
        HelpFormatter::printVersion();
        return 0;
    }

    FifoEngine engine(opts);
    return engine.execute();
}
