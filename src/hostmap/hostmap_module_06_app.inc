// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class HostmapApplication {
public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "[-] Error: Failed to initialize Winsock.\n";
            return 1;
        }

        HostmapOptions opts;
        if (!opts.Parse(argc, argv)) {
            std::cerr << "Use 'hostmap --help' for usage.\n";
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp();
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.jsonOutput) {
            HostmapReporter::RenderJson();
        } else {
            HostmapReporter::RenderText(opts);
        }

        return 0;
    }
};
