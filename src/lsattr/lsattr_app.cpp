#include "lsattr_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>

int LsattrApplication::Run(int argc, wchar_t* argv[]) const {
    LsattrOptions options;
    if (!options.Parse(argc, argv)) {
        options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"lsattr");
        return 1;
    }

    if (options.showHelp) {
        options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"lsattr");
        return 0;
    }
    if (options.showVersion) {
        options.PrintVersion();
        return 0;
    }

    std::vector<AttrRow> rows;
    bool allOk = true;

    for (const auto& path : options.paths) {
        if (!LsattrTraverser::Collect(path, options.recursive, rows, std::wcerr)) {
            allOk = false;
        }
    }

    LsattrReporter::PrintRows(rows, options.format);
    return allOk ? 0 : 1;
}
