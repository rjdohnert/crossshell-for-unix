#include "diskinfo_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>

int DiskinfoApplication::Run(int argc, wchar_t* argv[]) const {
    DiskinfoOptions opts;
    const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"diskinfo";

    if (!opts.Parse(argc, argv)) {
        opts.PrintHelp(progName);
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintHelp(progName);
        return 0;
    }
    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    auto rows = BlockDeviceInspector::EnumerateRows();
    DiskinfoReporter::Report(rows, opts);

    return 0;
}
