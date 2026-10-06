#include "pskill_app.hpp"

int PskillApplication::Run(int argc, wchar_t* argv[]) const {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    PskillOptions options;
    int parseResult = options.Parse(argc, argv);
    if (parseResult >= 0) {
        if (options.showHelp) {
            options.PrintHelp();
            return (argc < 2) ? EXIT_INVALID_ARGS : EXIT_SUCCESS_OK;
        }
        return parseResult;
    }

    return ProcessKillerEngine::Execute(options);
}
