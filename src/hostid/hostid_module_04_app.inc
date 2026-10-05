// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class HostidApplication {
public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        HostidOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage();
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage();
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        uint32_t id = HostIdentifierGenerator::ComputeHostId();
        HostidReporter::Emit(opts, id);
        return 0;
    }
};
