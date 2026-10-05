    // =========================================================================
    // Application Lifecycle Controller
    // =========================================================================
    class GetfaclApp {
    public:
        static int run(int argc, char* argv[]) {
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            try {
                Options opts = ArgumentParser::parse(argc, argv);

                if (opts.show_help) {
                    ArgumentParser::print_help(std::cout);
                    return EXIT_SUCCESS;
                }

                if (opts.show_version) {
                    ArgumentParser::print_version(std::cout);
                    return EXIT_SUCCESS;
                }

                // If stdin is redirected and no paths specified, read from stdin
                if (opts.paths.empty() && !_isatty(_fileno(stdin))) {
                    opts.read_stdin = true;
                }

                if (opts.paths.empty() && !opts.read_stdin) {
                    std::cerr << "getfacl: missing operand\n";
                    std::cerr << "Try 'getfacl --help' for more information.\n";
                    return EXIT_FAILURE;
                }

                int exit_code = EXIT_SUCCESS;

                // Process files from stdin
                if (opts.read_stdin) {
                    std::string line;
                    while (std::getline(std::cin, line)) {
                        if (line.empty()) continue;
                        if (!process_path(line, opts)) {
                            exit_code = EXIT_FAILURE;
                        }
                    }
                }

                // Process explicit CLI paths
                for (const auto& path : opts.paths) {
                    if (path == "-") {
                        std::string line;
                        while (std::getline(std::cin, line)) {
                            if (line.empty()) continue;
                            if (!process_path(line, opts)) {
                                exit_code = EXIT_FAILURE;
                            }
                        }
                    } else {
                        if (!process_path(path, opts)) {
                            exit_code = EXIT_FAILURE;
                        }
                    }
                }

                return exit_code;

            } catch (const std::exception& ex) {
                std::cerr << "getfacl: error: " << ex.what() << "\n";
                return EXIT_FAILURE;
            }
        }

    private:
        static bool process_path(const std::string& path_str, const Options& opts) {
            fs::path p(path_str);
            std::error_code ec;

            if (!fs::exists(p, ec)) {
                std::cerr << "getfacl: " << path_str << ": No such file or directory\n";
                return false;
            }

            bool success = true;

            if (opts.recursive && fs::is_directory(p, ec)) {
                auto dir_opts = opts.follow_symlinks
                    ? fs::directory_options::follow_directory_symlink
                    : fs::directory_options::none;

                FileAclInfo root_info;
                if (SecurityInspector::inspect_file(p, opts, root_info)) {
                    AclFormatter::format(std::cout, root_info, opts);
                } else {
                    std::cerr << "getfacl: " << p.string() << ": Access is denied\n";
                    success = false;
                }

                for (auto it = fs::recursive_directory_iterator(p, dir_opts, ec);
                     it != fs::recursive_directory_iterator(); it.increment(ec)) {
                    if (ec) {
                        std::cerr << "getfacl: " << it->path().string() << ": " << ec.message() << "\n";
                        ec.clear();
                        success = false;
                        continue;
                    }

                    FileAclInfo item_info;
                    if (SecurityInspector::inspect_file(it->path(), opts, item_info)) {
                        AclFormatter::format(std::cout, item_info, opts);
                    } else {
                        std::cerr << "getfacl: " << it->path().string() << ": Access is denied\n";
                        success = false;
                    }
                }
            } else {
                FileAclInfo info;
                if (SecurityInspector::inspect_file(p, opts, info)) {
                    AclFormatter::format(std::cout, info, opts);
                } else {
                    std::cerr << "getfacl: " << path_str << ": Access is denied\n";
                    return false;
                }
            }

            return success;
        }
    };
