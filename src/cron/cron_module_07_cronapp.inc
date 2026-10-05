class CronApp {
public:
    static int run(int argc, char* argv[]) {
        CronOptions options;
        if (!CronOptions::parse(argc, argv, options)) {
            return 1;
        }
        CronEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return CronApp::run(argc, argv);
}
