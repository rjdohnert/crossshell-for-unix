#include "ptime_app.hpp"

int PtimeApplication::Run(int argc, wchar_t* argv[]) const {
    TimeOptions options;
    if (!options.Parse(argc, argv)) {
        return 1;
    }

    if (options.showHelp) {
        options.PrintHelp();
        return 0;
    }

    if (options.showVersion) {
        options.PrintVersion();
        return 0;
    }

    ProcessTimeMetrics metrics;
    int execResult = ProcessTimerEngine::ExecuteAndMeasure(argc, argv, options, metrics);
    if (execResult != 0) {
        return execResult;
    }

    PtimeReporter::Report(options, metrics);
    return static_cast<int>(metrics.exitCode);
}
