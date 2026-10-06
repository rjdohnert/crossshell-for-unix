#include "nice_app.hpp"

int NiceApplication::Run(int argc, wchar_t* argv[]) const {
    NiceOptions options;
    if (!options.Parse(argc, argv)) {
        return 1;
    }

    if (options.showHelp) {
        options.PrintUsage(argv[0]);
        return 0;
    }

    if (options.showVersion) {
        options.PrintVersion();
        return 0;
    }

    return ProcessPriorityEngine::ExecuteWithPriority(argc, argv, options);
}
