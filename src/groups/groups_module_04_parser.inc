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
                } else if (arg == "-s" || arg == "--sid") {
                    options.showSid = true;
                } else if (arg == "-t" || arg == "--type") {
                    options.showType = true;
                } else if (arg == "-q" || arg == "--quiet") {
                    options.quiet = true;
                } else if (arg == "-v" || arg == "--verbose") {
                    options.verbose = true;
                } else if (arg == "-d" || arg == "--delimiter") {
                    if (i + 1 < argc) {
                        options.delimiter = argv[++i];
                        if (options.delimiter == "\\n") options.delimiter = "\n";
                        if (options.delimiter == "\\t") options.delimiter = "\t";
                    } else {
                        std::cerr << "groups: option '" << arg << "' requires an argument\n";
                    }
                } else if (arg == "-") {
                    options.readFromStdin = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "groups: unrecognized option '" << arg << "'\n";
                    std::cerr << "Try 'groups --help' for more information.\n";
                } else {
                    options.usernames.emplace_back(arg);
                }
            }

            // If no usernames provided and standard input is redirected/piped
            if (options.usernames.empty() && !IsTerminalInput()) {
                options.readFromStdin = true;
            }

            return options;
        }

        static bool IsTerminalInput() {
            return _isatty(_fileno(stdin)) != 0;
        }
    };

    // Application orchestrator
