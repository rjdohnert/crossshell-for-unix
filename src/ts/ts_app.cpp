#include "ts_app.hpp"
#include "ts_engine.hpp"
#include "ts_options.hpp"

int TsApp::run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        TsOptions options;
        if (!TsOptions::parse(argc, argv, options)) {
            return 1;
        }

        TsEngine engine(std::move(options));
        return engine.execute();
    }
