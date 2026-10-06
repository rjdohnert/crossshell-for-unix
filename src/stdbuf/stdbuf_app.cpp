#include "stdbuf_app.hpp"
#include "stdbuf_options.hpp"
#include "stdbuf_process_launcher.hpp"

int StdbufApp::run(int argc, wchar_t* argv[]) {
        StdbufOptions options;
        if (!StdbufOptions::parse(argc, argv, options)) {
            return 1;
        }
        return StdbufProcessLauncher::launch(options);
    }
