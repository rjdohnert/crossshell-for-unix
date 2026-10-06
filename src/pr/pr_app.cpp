#include "pr_app.hpp"

int PrApp::run(int argc, wchar_t* argv[]) {
    PrOptions options;
    if (!PrOptions::parse(argc, argv, options)) {
        return 1;
    }
    PrEngine engine(std::move(options));
    return engine.execute();
}
