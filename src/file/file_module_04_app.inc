class FileApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        FileOptions options;
        if (!FileOptions::parse(argc, argv, options)) {
            return 1;
        }
        FileEngine engine(std::move(options));
        return engine.execute();
    }
};
