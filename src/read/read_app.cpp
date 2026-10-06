#include "read_app.hpp"

int ReadApp::run(int argc, wchar_t* argv[]) {
    ReadOptions options;
    if (!ReadOptions::parse(argc, argv, options)) {
        return 1;
    }
    ReadEngine engine(std::move(options));
    return engine.execute();
}
