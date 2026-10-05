// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class FindmntApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        FindmntOptions opts;
        const wchar_t* progName = (argc > 0 && argv[0]) ? argv[0] : L"findmnt";

        if (!opts.Parse(argc, argv)) {
            opts.PrintHelp(progName);
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp(progName);
            return 0;
        }
        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        auto entries = MountInspector::EnumerateMounts();
        FindmntReporter::Report(entries, opts);

        return 0;
    }
};
