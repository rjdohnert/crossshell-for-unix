#include "strace_app.hpp"

int StraceApplication::Run(int argc, wchar_t* argv[]) const {
    TraceOptions options;
    if (!options.Parse(argc, argv)) {
        return 1;
    }

    return TraceEngine::Execute(options);
}
