class FdApplication {
public:
    int Run(int argc, char* argv[]) const {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        ConsoleEnvironment::InitConsole();
        FdOptions config;

        if (!config.Parse(argc, argv)) {
            return 1;
        }

        if (config.showHelp) {
            config.PrintHelp();
            return 0;
        }
        if (config.showVersion) {
            config.PrintVersion();
            return 0;
        }

        CompiledPattern compiledPattern = CompiledPatternMatcher::PreparePattern(config.pattern, config.caseMode, config.fixedStrings);

        std::vector<std::string> matches;
        for (const auto& path : config.searchPaths) {
            FdSearchEngine::SearchDirectory(path, config, compiledPattern, matches);
        }

        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
            std::ostringstream output;
            if (config.outputFormat == OutputFormat::Json) {
                output << "[";
                for (size_t i = 0; i < matches.size(); ++i) {
                    if (i) output << ",";
                    output << "{\"path\":\"" << matches[i] << "\"}";
                }
                output << "]\n";
            } else if (config.outputFormat == OutputFormat::Csv) {
                output << "path\n";
                for (const auto& match : matches) output << match << "\n";
            } else {
                output << "PATH\n----\n";
                for (const auto& match : matches) output << match << "\n";
            }

            std::string text = output.str();
            if (!config.pipeCommand.empty()) {
                FILE* pipe = _popen(config.pipeCommand.c_str(), "w");
                if (pipe) {
                    fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else {
                std::cout << text;
            }
        }

        return 0;
    }
};
