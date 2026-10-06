#include "refresh_app.hpp"
#include "refresh_options.hpp"
#include "service_subsystem_manager.hpp"

int RefreshApplication::Run(int argc, wchar_t* argv[]) const {
        SetConsoleOutputCP(CP_UTF8);

        RefreshOptions options;
        if (!options.Parse(argc, argv)) {
            std::cerr << "Try 'refresh --help' for usage.\n";
            return 2;
        }

        if (options.showHelp) {
            options.PrintHelp();
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        return ServiceSubsystemManager::RefreshService(options);
    }
