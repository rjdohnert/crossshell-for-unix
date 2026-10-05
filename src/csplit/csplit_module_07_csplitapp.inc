class CsplitApp {
public:
    static int run(int argc, char* argv[]) {
        CsplitOptions options;
        if (!CsplitOptions::parse(argc, argv, options)) {
            return 1;
        }
        CsplitEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return CsplitApp::run(argc, argv);
}
