#include "portmap_app.hpp"

int PortmapApplication::Run(int argc, char* argv[]) {
    WinsockScope winsock;
    if (!winsock.IsInitialized()) {
        std::cerr << "[-] Error: Failed to initialize Winsock.\n";
        return 1;
    }

    PortmapOptions cfg;
    if (!cfg.Parse(argc, argv)) {
        std::cerr << "Use 'portmap --help' for usage.\n";
        return 1;
    }

    if (cfg.showHelp) {
        cfg.PrintHelp();
        return 0;
    }

    if (cfg.showVersion) {
        cfg.PrintVersion();
        return 0;
    }

    std::vector<ConnectionEntry> entries;
    PortTableCollector::CollectAll(entries, cfg);
    PortmapReporter::Emit(cfg, entries);

    return 0;
}
