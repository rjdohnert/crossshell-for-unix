    // =========================================================================
    // Main Application Coordinator
    // =========================================================================
    class DuApp {
    public:
        static int run(int argc, char* argv[]) {
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            Options opts;
            if (!ArgParser::parse(argc, argv, opts)) {
                return static_cast<int>(ExitCode::Failure);
            }

            if (opts.show_help) {
                HelpSystem::print_help();
                return static_cast<int>(ExitCode::Success);
            }

            if (opts.show_version) {
                HelpSystem::print_version();
                return static_cast<int>(ExitCode::Success);
            }

            std::vector<std::string> final_paths;

            // Ingestion via --files0-from
            if (!opts.files0_from.empty()) {
                if (opts.files0_from == "-") {
#ifdef _WIN32
                    if (_setmode(_fileno(stdin), _O_BINARY) == -1) return static_cast<int>(ExitCode::Failure);
#endif
                    final_paths = InputStreamReader::read_paths_from_stream(std::cin, '\0');
                } else {
                    std::ifstream f(opts.files0_from, std::ios::binary);
                    if (!f.is_open()) {
                        std::cerr << "du: cannot open '" << opts.files0_from << "' for reading: No such file or directory\n";
                        return static_cast<int>(ExitCode::Failure);
                    }
                    final_paths = InputStreamReader::read_paths_from_stream(f, '\0');
                }
            } else {
                // Ingestion via positional arguments or stdin '-'
                if (opts.paths.empty()) {
                    final_paths.push_back(".");
                } else {
                    for (const auto& p : opts.paths) {
                        if (p == "-") {
#ifdef _WIN32
                            if (_setmode(_fileno(stdin), _O_BINARY) == -1) return static_cast<int>(ExitCode::Failure);
#endif
                            auto piped_paths = InputStreamReader::read_paths_from_stream(std::cin, '\n');
                            final_paths.insert(final_paths.end(), piped_paths.begin(), piped_paths.end());
                        } else {
                            final_paths.push_back(p);
                        }
                    }
                }
            }

            DiskUsageScanner scanner(opts);
            std::ostringstream captured;
            std::streambuf* oldOutput = nullptr;
            if (opts.output_format || !opts.pipe_command.empty()) oldOutput = std::cout.rdbuf(captured.rdbuf());
            bool success = scanner.scan_all(final_paths);
            if (oldOutput) {
                std::cout.rdbuf(oldOutput);
                std::string data = captured.str();
                std::string text = opts.output_format == 1 ? "{\"output\":\"" + data + "\"}\n" : opts.output_format == 2 ? "\"output\"\n\"" + data + "\"\n" : "OUTPUT\n------\n" + data;
                if (!opts.pipe_command.empty()) { FILE* pipe = _popen(opts.pipe_command.c_str(), "w"); if (pipe) { std::fwrite(text.data(), 1, text.size(), pipe); _pclose(pipe); } }
                else std::cout << text;
            }
            return success ? static_cast<int>(ExitCode::Success) : static_cast<int>(ExitCode::Failure);
        }
    };
}
