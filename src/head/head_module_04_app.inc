// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class HeadApp {
public:
    static int run(int argc, char* argv[]) {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

        HeadOptions options;
        if (!HeadOptions::parse(argc, argv, options)) {
            return 1;
        }

        HeadEngine engine(std::move(options));
        return engine.execute();
    }
};
