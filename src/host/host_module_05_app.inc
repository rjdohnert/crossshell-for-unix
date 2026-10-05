// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class HostApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        NativePipeConfigurator::Configure();

        HostOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"host");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"host");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::wcerr << L"host: winsock initialization failed\n";
            return 1;
        }

        HostResolver resolver;
        std::vector<ResolvedRecord> records;
        if (!resolver.Resolve(opts.target, records)) {
            std::wcerr << L"Host " << opts.target << L" not found\n";
            return 1;
        }

        HostReporter::Emit(opts, records);
        return 0;
    }
};
