    class ArgParser {
    public:
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
                } else if (arg == "--check-order") {
                    opts.check_order = true;
                } else if (arg == "--nocheck-order") {
                    opts.check_order = false;
                } else if (arg == "-i" || arg == "--ignore-case") {
                    opts.ignore_case = true;
                } else if (arg == "-z" || arg == "--zero-terminated") {
                    opts.delimiter = '\0';
                } else if (arg == "--total") {
                    opts.show_total = true;
                } else if (arg.rfind("--output-delimiter=", 0) == 0) {
                    opts.col_delimiter = arg.substr(19);
                } else if (arg == "--output-delimiter") {
                    if (i + 1 < argc) {
                        opts.col_delimiter = argv[++i];
                    } else {
                        std::cerr << "comm: option '--output-delimiter' requires an argument\n";
                        return false;
                    }
                } else if (arg.length() > 1 && arg[0] == '-' && arg != "-") {
                    // Handle clustered flags: -1, -2, -3, -12, -13, -23, -123, -i, -z
                    bool valid_cluster = true;
                    for (size_t j = 1; j < arg.length(); ++j) {
                        switch (arg[j]) {
                            case '1': opts.show_col1 = false; break;
                            case '2': opts.show_col2 = false; break;
                            case '3': opts.show_col3 = false; break;
                            case 'i': opts.ignore_case = true; break;
                            case 'z': opts.delimiter = '\0'; break;
                            default:
                                valid_cluster = false;
                                break;
                        }
                    }
                    if (!valid_cluster) {
                        std::cerr << "comm: unrecognized option '" << arg << "'\n"
                                  << "Try 'comm --help' for more information.\n";
                        return false;
                    }
                } else {
                    positional.push_back(arg);
                }
            }

            if (opts.show_help || opts.show_version) {
                return true;
            }

            if (positional.size() < 2) {
                std::cerr << "comm: missing operand" << (positional.empty() ? "" : " after '" + positional[0] + "'") << "\n"
                          << "Try 'comm --help' for more information.\n";
                return false;
            }

            if (positional.size() > 2) {
                std::cerr << "comm: extra operand '" << positional[2] << "'\n"
                          << "Try 'comm --help' for more information.\n";
                return false;
            }

            opts.file1 = positional[0];
            opts.file2 = positional[1];
            return true;
        }
    };

    // =========================================================================
    // Help & Information Subsystem
    // =========================================================================
