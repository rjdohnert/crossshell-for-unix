class EntabApp {
public:
    static int run(int argc, char* argv[]) {
        if (argc == 1 && _isatty(_fileno(stdin))) {
            EntabOptions::printHelp(argv[0]);
            return 0;
        }

        EntabOptions options;
        if (!EntabOptions::parse(argc, argv, options)) {
            return 1;
        }

        EntabEngine engine(std::move(options));
        return engine.execute(argv[0]);
    }
};
