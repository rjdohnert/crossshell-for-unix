#include "backend_config.hpp"
#include "xz_app.hpp"
#include "xz_backend_finder.hpp"
#include "xz_options.hpp"
#include "xz_process_runner.hpp"

int XzApp::run(int argc, wchar_t* argv[]) {
        XzOptions options;
        bool showHelp = false;
        bool showVersion = false;

        if (!XzOptions::parse(argc, argv, options, showHelp, showVersion)) {
            return 1;
        }

        if (showHelp) {
            XzOptions::printHelp();
            return 0;
        }

        BackendConfig backend = XzBackendFinder::discoverBackend();

        if (showVersion) {
            XzOptions::printVersion(backend);
            return 0;
        }

        if (backend.applicationPath.empty()) {
            std::wcerr << L"xz: no suitable backend found (requires xz.exe or 7z.exe in PATH)\n";
            return 127;
        }

        return XzProcessRunner::execute(backend, options.forwardedArgs);
    }
