#include "process_stopper_engine.hpp"
#include "signal_parser.hpp"
#include "stop_app.hpp"
#include "stop_options.hpp"

int StopApplication::Run(int argc, char* argv[]) const {
        StopOptions options;
        int parseResult = options.Parse(argc, argv);
        if (parseResult >= 0) {
            if (options.showHelp) {
                options.PrintUsage();
                return (argc < 2) ? 1 : 0;
            }
            if (options.showVersion) {
                options.PrintVersion();
                return 0;
            }
            if (options.showSignalList) {
                SignalParser::PrintSignalList();
                return 0;
            }
            return parseResult;
        }

        return ProcessStopperEngine::Execute(options);
    }
