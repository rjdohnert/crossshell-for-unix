#include "cron_app.h"
#include "cron_options.h"
#include "cron_engine.h"

int CronApp::run(int argc, char* argv[]) {
    CronOptions options;
    if (!CronOptions::parse(argc, argv, options)) {
        return 1;
    }
    CronEngine engine(std::move(options));
    return engine.execute();
}
