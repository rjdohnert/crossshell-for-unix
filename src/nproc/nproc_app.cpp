#include "nproc_app.hpp"

int NprocApp::run(int argc, char* argv[]) {
    NprocOptions options;
    if (!NprocOptions::parse(argc, argv, options)) {
        return 1;
    }
    NprocEngine engine(std::move(options));
    return engine.execute();
}
