#include "tty_app.hpp"
#include "tty_engine.hpp"
#include "tty_options.hpp"

int TtyApp::run(int argc, wchar_t* argv[]) {
        TtyOptions options;
        if (!TtyOptions::parse(argc, argv, options)) {
            return 2;
        }
        return TtyEngine::execute(options);
    }
