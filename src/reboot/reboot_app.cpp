#include "reboot_app.hpp"
#include "reboot_engine.hpp"
#include "reboot_options.hpp"

int RebootApplication::Run(int argc, char* argv[]) const {
        RebootOptions options;
        int parseResult = options.Parse(argc, argv);
        if (parseResult >= 0) {
            if (options.showHelp) {
                options.PrintUsage();
                return 0;
            }
            if (options.showVersion) {
                options.PrintVersion();
                return 0;
            }
            return parseResult;
        }

        return RebootEngine::Execute(options);
    }
