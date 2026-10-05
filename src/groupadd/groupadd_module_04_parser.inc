    // Command line argument parser
    class CommandLineParser {
    public:
        static CommandOptions Parse(int argc, char* argv[]) {
            CommandOptions options;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    options.showHelp = true;
                    return options;
                } else if (arg == "-V" || arg == "--version") {
                    options.showVersion = true;
                    return options;
                } else if (arg == "-f" || arg == "--force") {
                    options.force = true;
                } else if (arg == "-r" || arg == "--system") {
                    options.systemGroup = true;
                } else if (arg == "-q" || arg == "--quiet") {
                    options.quiet = true;
                } else if (arg == "-v" || arg == "--verbose") {
                    options.verbose = true;
                } else if (arg == "-c" || arg == "--comment") {
                    if (i + 1 < argc) {
                        options.comment = argv[++i];
                    } else {
                        std::cerr << "groupadd: option '" << arg << "' requires an argument\n";
                    }
                } else if (arg == "-") {
                    options.readFromStdin = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "groupadd: unrecognized option '" << arg << "'\n";
                    std::cerr << "Try 'groupadd --help' for more information.\n";
                } else {
                    if (options.groupName.empty()) {
                        options.groupName = arg;
                    } else {
                        std::cerr << "groupadd: extra operand '" << arg << "'\n";
                    }
                }
            }

            // Detect if stdin has redirected input / pipe
            if (options.groupName.empty() && !IsTerminalInput()) {
                options.readFromStdin = true;
            }

            return options;
        }

        static bool IsTerminalInput() {
            return _isatty(_fileno(stdin)) != 0;
        }
    };

