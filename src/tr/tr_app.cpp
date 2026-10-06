#include "tr_app.hpp"
#include "tr_engine.hpp"
#include "tr_options.hpp"

int TrApp::run(int argc, char* argv[]) {
        TrOptions options;
        if (!TrOptions::parse(argc, argv, options)) {
            return 1;
        }

        TrEngine engine(std::move(options));
        return engine.execute();
    }
