#include "od_app.hpp"

int OdApp::run(int argc, char* argv[]) {
    OdOptions options;
    if (!OdOptions::parse(argc, argv, options)) {
        return 1;
    }
    OdEngine engine(std::move(options));
    return engine.execute();
}
