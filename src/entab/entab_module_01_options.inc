class EntabOptions {
public:
    static constexpr const char* VERSION = "1.0.0";
    static constexpr size_t IO_BUFFER_SIZE = 64 * 1024;

    bool convertAll{false};          // -a, --all (convert all spaces, not just initial)
    bool explicitFirstOnly{false};   // --first-only (override -a)
    std::vector<size_t> tabStops;    // -t LIST (explicit 1-based column stops)
    size_t singleTabWidth{8};        // -t N or -N (default is 8)
    std::vector<std::string> files;  // File list (or stdin if empty/'-')
    bool showHelp{false};
    bool showVersion{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp(const char* progName) {
        std::cout << R"(entab(1)                  CrossShell for UNIX Reference Manual                          entab(1)

NAME
     entab - replace groups of spaces with tabs

SYNOPSIS
     entab [OPTION]... [FILE]...

DESCRIPTION
     Convert spaces in each FILE to tabs, writing to standard output.
     With no FILE, or when FILE is -, read standard input.

OPTIONS
     -a, --all
          Convert all sequences of spaces, not just initial whitespace.

     --first-only
          Convert only leading sequences of whitespace (overrides -a).

     -t, --tabs=N
          Have tabs N characters apart (default is 8). Enables -a.

     -t, --tabs=LIST
          Use a comma- or space-separated list of explicit tab stop column
          positions (1-based). Enables -a.

     -N
          Shorthand for -t N (e.g., -4 sets tabs 4 spaces apart).

     -h, --help
          Display this reference manual page and exit.

     -v, --version
          Output version information and exit.

     --json
          Output converted text as JSON.

     --csv
          Output converted text as CSV.

     --table
          Output converted text as a table.

     --pipe COMMAND
          Send converted text through COMMAND.

     --
          Explicitly treat all subsequent arguments as input file names, even
          if they start with '-'.

EXAMPLES
     entab file.txt
     entab -4 data.txt
     entab --first-only source.txt > output.txt
     entab --json report.txt

CrossShell for UNIX                                               entab(1)
)";
    }

    static void printVersion() {
        std::cout << "unexpand version " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

    static bool parseTabSpec(const std::string& spec, EntabOptions& opts) {
        std::vector<size_t> stops;
        std::string token;

        for (char c : spec) {
            if (c == ',' || c == ' ' || c == '\t') {
                if (!token.empty()) {
                    try {
                        unsigned long long val = std::stoull(token);
                        if (val == 0) return false;
                        stops.push_back(static_cast<size_t>(val));
                    } catch (...) {
                        return false;
                    }
                    token.clear();
                }
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                token += c;
            } else {
                return false;
            }
        }

        if (!token.empty()) {
            try {
                unsigned long long val = std::stoull(token);
                if (val == 0) return false;
                stops.push_back(static_cast<size_t>(val));
            } catch (...) {
                return false;
            }
        }

        if (stops.empty()) return false;

        if (stops.size() == 1) {
            opts.singleTabWidth = stops[0];
            opts.tabStops.clear();
        } else {
            opts.tabStops = stops;
            opts.singleTabWidth = 8;
        }
        return true;
    }

    static bool parse(int argc, char* argv[], EntabOptions& opts) {
        bool rawFilesOnly = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (rawFilesOnly) {
                opts.files.push_back(arg);
                continue;
            }

            if (arg == "--") {
                rawFilesOnly = true;
                continue;
            }

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                opts.showHelp = true;
                return true;
            }

            if (arg == "-v" || arg == "--version") {
                opts.showVersion = true;
                return true;
            }
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

            if (arg == "-a" || arg == "--all") {
                opts.convertAll = true;
                opts.explicitFirstOnly = false;
                continue;
            }

            if (arg == "--first-only") {
                opts.convertAll = false;
                opts.explicitFirstOnly = true;
                continue;
            }

            if (arg.rfind("--tabs=", 0) == 0) {
                if (!parseTabSpec(arg.substr(7), opts)) {
                    std::cerr << "unexpand: invalid tab spec: '" << arg.substr(7) << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg == "-t" || arg == "--tabs") {
                if (i + 1 >= argc) {
                    std::cerr << "unexpand: option requires an argument -- '" << arg << "'\n";
                    return false;
                }
                if (!parseTabSpec(argv[++i], opts)) {
                    std::cerr << "unexpand: invalid tab spec: '" << argv[i] << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg.rfind("-t", 0) == 0 && arg.size() > 2) {
                if (!parseTabSpec(arg.substr(2), opts)) {
                    std::cerr << "unexpand: invalid tab spec: '" << arg.substr(2) << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg.size() > 1 && arg[0] == '-' && std::isdigit(static_cast<unsigned char>(arg[1]))) {
                if (!parseTabSpec(arg.substr(1), opts)) {
                    std::cerr << "unexpand: invalid tab size: '" << arg.substr(1) << "'\n";
                    return false;
                }
                if (!opts.explicitFirstOnly) opts.convertAll = true;
                continue;
            }

            if (arg.size() > 0 && arg[0] == '-' && arg != "-") {
                std::cerr << "unexpand: invalid option -- '" << arg << "'\n";
                std::cerr << "Try 'unexpand --help' for more information.\n";
                return false;
            }

            opts.files.push_back(arg);
        }

        return true;
    }
};
