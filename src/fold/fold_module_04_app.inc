    // =========================================================================
    // Application Controller
    // =========================================================================
    class FoldApp {
    public:
        static int run(int argc, char* argv[]) {
            // Enable binary transparency on standard Windows I/O handles
            #ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
            _setmode(_fileno(stdout), _O_BINARY);
            #endif

            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            try {
                Options options = ArgumentParser::parse(argc, argv);

                if (options.show_help) {
                    ArgumentParser::print_help(std::cout);
                    return EXIT_SUCCESS;
                }

                if (options.show_version) {
                    ArgumentParser::print_version(std::cout);
                    return EXIT_SUCCESS;
                }

                TextFolder folder(options);
                std::ostringstream captured;
                std::ostream& output = (options.output_format != 0 || !options.pipe_command.empty()) ? static_cast<std::ostream&>(captured) : std::cout;

                // Pipe handling: If no input files given or "-" specified, process stdin
                if (options.files.empty()) {
                    folder.process_stream(std::cin, output);
                    return EXIT_SUCCESS;
                }

                int exit_code = EXIT_SUCCESS;
                for (const auto& file_path : options.files) {
                    if (file_path == "-") {
                        folder.process_stream(std::cin, output);
                    } else {
                        std::ifstream infile(file_path, std::ios::binary);
                        if (!infile.is_open()) {
                            std::cerr << "fold: cannot open '" << file_path << "': No such file or directory\n";
                            exit_code = EXIT_FAILURE;
                            continue;
                        }
                        folder.process_stream(infile, output);
                    }
                }
                if (options.output_format != 0 || !options.pipe_command.empty()) {
                    std::istringstream lines(captured.str());
                    std::ostringstream formatted;
                    if (options.output_format == 1) {
                        formatted << "[";
                        std::string line; bool first = true;
                        while (std::getline(lines, line)) { if (!first) formatted << ","; first = false; formatted << "{\"line\":\"" << line << "\"}"; }
                        formatted << "]\n";
                    } else if (options.output_format == 2) {
                        formatted << "line\n" << captured.str();
                    } else {
                        formatted << "LINE\n----\n" << captured.str();
                    }
                    std::string text = formatted.str();
                    if (!options.pipe_command.empty()) {
                        FILE* pipe = _popen(options.pipe_command.c_str(), "w");
                        if (!pipe) return EXIT_FAILURE;
                        fwrite(text.data(), 1, text.size(), pipe);
                        _pclose(pipe);
                    } else {
                        std::cout << text;
                    }
                }
                return exit_code;

            } catch (const std::exception& ex) {
                std::cerr << "fold: error: " << ex.what() << "\n";
                std::cerr << "Try 'fold --help' for more information.\n";
                return EXIT_FAILURE;
            }
        }
    };
