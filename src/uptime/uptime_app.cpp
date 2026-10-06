#include "uptime_app.hpp"
#include "uptime_engine.hpp"
#include "uptime_options.hpp"

int UptimeApp::run(int argc, char* argv[]) {
        UptimeOptions options;
        if (!UptimeOptions::parse(argc, argv, options)) {
            return 1;
        }
        UptimeEngine engine(std::move(options));
        return engine.execute();
    }
