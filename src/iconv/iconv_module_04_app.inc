// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class IconvApp {
public:
    static int run(int argc, char* argv[]) {
        IconvOptions options;
        if (!IconvOptions::parse(argc, argv, options)) {
            return 1;
        }
        IconvEngine engine(std::move(options));
        return engine.execute();
    }
};
