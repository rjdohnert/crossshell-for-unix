    class ArgParser {
    public:
        static uint64_t parse_size(const std::string& str) {
            uint64_t multiplier = 1;
            std::string num_part = str;

            if (!str.empty()) {
                char suffix = static_cast<char>(std::tolower(str.back()));
                if (suffix == 'k') { multiplier = 1024ULL; num_part.pop_back(); }
                else if (suffix == 'm') { multiplier = 1024ULL * 1024ULL; num_part.pop_back(); }
                else if (suffix == 'g') { multiplier = 1024ULL * 1024ULL * 1024ULL; num_part.pop_back(); }
            }

            try {
                size_t idx = 0;
                uint64_t val = std::stoull(num_part, &idx, 0); // Base 0 supports decimal, octal (0...), hex (0x...)
                return val * multiplier;
            } catch (...) {
                return 0;
            }
        }

        static bool parse(int argc, char* argv[], Options& opts) {
            std::vector<std::string> positional;

            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];

                if (arg == "--help" || arg == "-h" || arg == "-?") {
                    opts.show_help = true;
                    return true;
                } else if (arg == "--version" || arg == "-v") {
                    opts.show_version = true;
                    return true;
                } else if (arg == "-b" || arg == "--print-bytes") {
                    opts.print_bytes = true;
                } else if (arg == "-l" || arg == "--verbose") {
                    opts.verbose_all = true;
                } else if (arg == "-s" || arg == "-q" || arg == "--silent" || arg == "--quiet") {
                    opts.silent = true;
                } else if (arg == "-n" || arg == "--bytes") {
                    if (i + 1 < argc) {
                        opts.limit_bytes = parse_size(argv[++i]);
                    } else {
                        std::cerr << "cmp: option requires an argument -- 'n'\n";
                        return false;
                    }
                } else if (arg.rfind("-n=", 0) == 0 || arg.rfind("--bytes=", 0) == 0) {
                    opts.limit_bytes = parse_size(arg.substr(arg.find('=') + 1));
                } else if (arg == "-i" || arg == "--ignore-initial") {
                    if (i + 1 < argc) {
                        parse_skip(argv[++i], opts);
                    } else {
                        std::cerr << "cmp: option requires an argument -- 'i'\n";
                        return false;
                    }
                } else if (arg.rfind("-i=", 0) == 0 || arg.rfind("--ignore-initial=", 0) == 0) {
                    parse_skip(arg.substr(arg.find('=') + 1), opts);
                } else if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
                    // Grouped flags, e.g. -bls
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case 'b': opts.print_bytes = true; break;
                            case 'l': opts.verbose_all = true; break;
                            case 's':
                            case 'q': opts.silent = true; break;
                            default:
                                std::cerr << "cmp: invalid option -- '" << arg[j] << "'\n";
                                return false;
                        }
                    }
                } else {
                    positional.push_back(arg);
                }
            }

            if (positional.empty()) {
                std::cerr << "cmp: missing operand\nTry 'cmp --help' for more information.\n";
                return false;
            }

            opts.file1 = positional[0];
            if (positional.size() > 1) {
                opts.file2 = positional[1];
            } else {
                std::cerr << "cmp: missing operand after '" << opts.file1 << "'\n";
                return false;
            }

            // Positional skips support: cmp FILE1 FILE2 [SKIP1 [SKIP2]]
            if (positional.size() > 2) {
                opts.skip1 = parse_size(positional[2]);
            }
            if (positional.size() > 3) {
                opts.skip2 = parse_size(positional[3]);
            }

            return true;
        }

    private:
        static void parse_skip(const std::string& val, Options& opts) {
            auto colon_pos = val.find(':');
            if (colon_pos != std::string::npos) {
                opts.skip1 = parse_size(val.substr(0, colon_pos));
                opts.skip2 = parse_size(val.substr(colon_pos + 1));
            } else {
                opts.skip1 = parse_size(val);
                opts.skip2 = opts.skip1;
            }
        }
    };

    // =========================================================================
    // Help & Version Information
    // =========================================================================
