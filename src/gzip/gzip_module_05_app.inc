// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class GzipApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        GzipOptions options;
        if (!GzipOptions::parse(argc, argv, options)) {
            return 1;
        }
        GzipEngine engine(std::move(options));
        return engine.execute();
    }
};
