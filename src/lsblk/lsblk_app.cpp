#include "lsblk_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>

int LsblkApplication::Run(int argc, wchar_t* argv[]) const {
    LsblkOptions opts;
    const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"lsblk";

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
    std::wcout << L"\n";
    LsblkReporter::Report(rows, opts);
    std::wcout << L"\n";

    return 0;
}
