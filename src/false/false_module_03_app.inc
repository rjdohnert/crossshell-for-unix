class FalseApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        FalseOptions options;
        if (!FalseOptions::parse(argc, argv, options)) {
            return 1;
        }
        return FalseReporter::report(options);
    }
};
