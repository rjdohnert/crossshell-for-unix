    // =========================================================================
    // Application Lifecycle Controller
    // =========================================================================
    class GetentApp {
    public:
        static int run(int argc, char* argv[]) {
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            DatabaseRegistry registry;
            Options opts = ArgumentParser::parse(argc, argv);

            if (opts.show_help) {
                ArgumentParser::print_help(std::cout, registry);
                return 0;
            }

            if (opts.show_version) {
                ArgumentParser::print_version(std::cout);
                return 0;
            }

            if (opts.database.empty()) {
                std::cerr << "getent: missing database operand\n";
                std::cerr << "Try 'getent --help' for more information.\n";
                return 1;
            }

            IDatabaseHandler* handler = registry.get(opts.database);
            if (!handler) {
                std::cerr << "getent: Unknown database: " << opts.database << "\n";
                return 1;
            }

            // Case A: Read keys from standard input pipeline
            if (opts.read_stdin || (!opts.keys.empty() && opts.keys.front() == "-")) {
                std::string key;
                bool all_found = true;
                while (std::cin >> key) {
                    if (!handler->lookup(key, std::cout)) {
                        all_found = false;
                    }
                    std::cout.flush();
                }
                return all_found ? 0 : 2;
            }

            // Case B: No keys specified -> Enumerate whole database
            if (opts.keys.empty()) {
                // If stdin is piped but no explicit keys, check if keys exist on stdin
                if (!_isatty(_fileno(stdin))) {
                    std::string key;
                    bool processed_any = false;
                    bool all_found = true;
                    while (std::cin >> key) {
                        processed_any = true;
                        if (!handler->lookup(key, std::cout)) {
                            all_found = false;
                        }
                        std::cout.flush();
                    }
                    if (processed_any) {
                        return all_found ? 0 : 2;
                    }
                }

                bool success = handler->enumerate(std::cout);
                std::cout.flush();
                return success ? 0 : 3;
            }

            // Case C: Explicit keys supplied on command-line
            bool all_found = true;
            for (const auto& key : opts.keys) {
                if (!handler->lookup(key, std::cout)) {
                    all_found = false;
                }
                std::cout.flush();
            }

            return all_found ? 0 : 2;
        }
    };
