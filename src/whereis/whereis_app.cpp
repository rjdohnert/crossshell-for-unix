#include "whereis_app.hpp"
#include "whereis_engine.hpp"
#include "whereis_options.hpp"

int WhereisApp::run(int argc, wchar_t* argv[]) {
        WhereisOptions options;
        if (!WhereisOptions::parse(argc, argv, options)) {
            return 1;
        }
        WhereisEngine engine(std::move(options));
        return engine.execute();
    }
