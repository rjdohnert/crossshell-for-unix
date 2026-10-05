// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class JoinApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        JoinOptions options;
        if (!JoinOptions::parse(argc, argv, options)) {
            return 1;
        }
        JoinEngine engine(std::move(options));
        return engine.execute();
    }
};
