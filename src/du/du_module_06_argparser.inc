    // =========================================================================
    // Command Line Parser
    // =========================================================================
    class ArgParser {
    public:
        static bool parse(int argc, char* argv[], Options& opts) {
            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];

                if (arg == "--help" || arg == "-h" || arg == "-?") {
                    opts.show_help = true;
                    return true;
                } else if (arg == "--version" || arg == "-v") {
                    opts.show_version = true;
                    return true;
                } else if (arg == "-H" || arg == "--human-readable") {
                    opts.format_mode = SizeFormatMode::HumanReadable;
                } else if (arg == "-a" || arg == "--all") {
                    opts.all_files = true;
                } else if (arg == "-s" || arg == "--summarize") {
                    opts.summarize_only = true;
                } else if (arg == "-c" || arg == "--total") {
                    opts.total = true;
                } else if (arg == "-k") {
                    opts.format_mode = SizeFormatMode::Kilobytes;
                    opts.block_size = 1024;
                } else if (arg == "-m") {
                    opts.format_mode = SizeFormatMode::Megabytes;
                    opts.block_size = 1024 * 1024;
                } else if (arg == "-b" || arg == "--bytes") {
                    opts.format_mode = SizeFormatMode::Bytes;
                    opts.apparent_size = true;
                } else if (arg == "--apparent-size") {
                    opts.apparent_size = true;
                } else if (arg == "-0" || arg == "--null") {
                    opts.null_terminated = true;
                } else if (arg == "-L" || arg == "--dereference") {
                    opts.dereference = true;
                } else if (arg == "-d" || arg == "--max-depth") {
                    if (i + 1 < argc) {
                        opts.max_depth = std::stoi(argv[++i]);
                    } else {
                        std::cerr << "du: option requires an argument -- 'd'\n";
                        return false;
                    }
                } else if (arg.rfind("-d=", 0) == 0 || arg.rfind("--max-depth=", 0) == 0) {
                    opts.max_depth = std::stoi(arg.substr(arg.find('=') + 1));
                } else if (arg.rfind("--exclude=", 0) == 0) {
                    opts.exclude_patterns.push_back(arg.substr(10));
                } else if (arg == "--exclude") {
                    if (i + 1 < argc) {
                        opts.exclude_patterns.push_back(argv[++i]);
                    } else {
                        std::cerr << "du: option '--exclude' requires an argument\n";
                        return false;
                    }
                } else if (arg.rfind("--files0-from=", 0) == 0) {
                    opts.files0_from = arg.substr(14);
                } else if (arg == "--json") {
                    opts.output_format = 1;
                } else if (arg == "--csv") {
                    opts.output_format = 2;
                } else if (arg == "--table") {
                    opts.output_format = 3;
                } else if (arg == "--pipe" && i + 1 < argc) {
                    opts.pipe_command = argv[++i];
                } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
                    // Clustered short options (e.g. -scH, -ak)
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case 'H': opts.format_mode = SizeFormatMode::HumanReadable; break;
                            case 'h': opts.show_help = true; return true;
                            case 'a': opts.all_files = true; break;
                            case 's': opts.summarize_only = true; break;
                            case 'c': opts.total = true; break;
                            case 'k': opts.format_mode = SizeFormatMode::Kilobytes; opts.block_size = 1024; break;
                            case 'm': opts.format_mode = SizeFormatMode::Megabytes; opts.block_size = 1024 * 1024; break;
                            case 'b': opts.format_mode = SizeFormatMode::Bytes; opts.apparent_size = true; break;
                            case '0': opts.null_terminated = true; break;
                            case 'L': opts.dereference = true; break;
                            default:
                                std::cerr << "du: invalid option -- '" << arg[j] << "'\n"
                                          << "Try 'du --help' for more information.\n";
                                return false;
                        }
                    }
                } else {
                    opts.paths.push_back(arg);
                }
            }
            return true;
        }
    };
