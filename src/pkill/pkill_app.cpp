#include "pkill_app.hpp"

int PkillApplication::Run(int argc, wchar_t* argv[]) const {
    PkillOptions options;
    int parseResult = options.Parse(argc, argv);
    if (parseResult >= 0) {
        if (options.showHelp) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pkill");
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }
        return parseResult;
    }

    return ProcessMatcherEngine::TerminateMatchingProcesses(options);
}
