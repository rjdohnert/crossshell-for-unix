// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class CalApp {
public:
    static int run(int argc, char* argv[]) {
        CalOptions options;
        if (!CalOptions::parse(argc, argv, options)) {
            return 1;
        }
        CalEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return CalApp::run(argc, argv);
}
