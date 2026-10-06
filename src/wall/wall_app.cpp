#include "wall_app.hpp"
#include "wall_config.hpp"
#include "wall_engine.hpp"

int WallApp::run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        WallConfig config;
        if (!WallConfig::parse(argc, argv, config)) {
            return 1;
        }

        WallEngine engine(std::move(config));
        return engine.execute();
    }
