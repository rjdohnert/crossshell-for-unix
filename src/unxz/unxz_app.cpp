#include "unxz_app.hpp"
#include "unxz_backend_config.hpp"
#include "unxz_backend_finder.hpp"
#include "unxz_options.hpp"
#include "unxz_process_runner.hpp"

int UnxzApp::run(int argc, wchar_t* argv[]) {
        UnxzOptions options;
        bool showHelp = false;
        bool showVersion = false;

        if (!UnxzOptions::parse(argc, argv, options, showHelp, showVersion)) {
            return 1;
        }

        if (showHelp) {
            UnxzOptions::printHelp();
            return 0;
        }

        UnxzBackendConfig backend = UnxzBackendFinder::discoverBackend();

        if (showVersion) {
            UnxzOptions::printVersion(backend);
            return 0;
        }

        if (backend.applicationPath.empty()) {
            std::wcerr << L"unxz: no suitable backend found (requires xz.exe or 7z.exe in PATH)\n";
            return 127;
        }

        return UnxzProcessRunner::execute(backend, options.forwardedArgs);
    }
