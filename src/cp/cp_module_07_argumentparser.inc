class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], CopyOptions& options) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                HelpFormatter::printHelp();
                exit(0);
            }
            if (arg == "--no-progress") {
                options.showProgress = false;
                continue;
            }
            if (arg == "--verbose") {
                options.verbose = true;
                continue;
            }
            if (arg == "--json") {
                options.outputJson = true;
                options.showProgress = false;
                continue;
            }

            if (arg.rfind("--", 0) == 0) {
                std::cerr << "cp: unrecognized option '" << arg << "'\nTry 'cp --help' for manual.\n";
                return false;
            }

            if (arg.length() > 1 && arg[0] == '-') {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'f':
                            options.force = true;
                            options.interactive = false;
                            break;
                        case 'i':
                            options.interactive = true;
                            options.force = false;
                            break;
                        case 'p':
                            options.preserve = true;
                            break;
                        case 'r':
                        case 'R':
                            options.recursive = true;
                            break;
                        case 'e':
                            options.hpuxExtentAcl = true;
                            break;
                        case 'P':
                            options.noDereference = true;
                            options.dereference = false;
                            break;
                        case 'L':
                            options.dereference = true;
                            options.noDereference = false;
                            break;
                        case 'v':
                            options.verbose = true;
                            break;
                        default:
                            std::cerr << "cp: illegal option -- " << arg[c] << "\n";
                            std::cerr << "usage: cp [-f | -i] [-p] [-e] [-P | -L] [-r | -R] file ... target\n";
                            return false;
                    }
                }
            } else {
                options.sources.push_back(fs::u8path(arg));
            }
        }

        if (options.sources.size() < 2) {
            std::cerr << "cp: missing operand\nTry 'cp --help' for more information.\n";
            return false;
        }

        options.destination = options.sources.back();
        options.sources.pop_back();

        return true;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    // Enable ANSI escape sequence processing on Windows consoles
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    CopyOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    CopyEngine engine(options);
    return engine.execute() ? 0 : 1;
}