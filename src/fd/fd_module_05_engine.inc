class FdSearchEngine {
public:
    static void SearchDirectory(const std::string& rootPath, const FdOptions& config, const CompiledPattern& compiledPattern, std::vector<std::string>& matches) {
        fs::path base(rootPath);
        if (!fs::exists(base)) {
            std::cerr << Color::RED << "Error: Search path '" << rootPath << "' does not exist." << Color::RESET << "\n";
            return;
        }

        auto options = fs::directory_options::skip_permission_denied;
        
        try {
            fs::recursive_directory_iterator it(base, options), end;
            while (it != end) {
                const auto& entry = *it;
                int currentDepth = it.depth();
                std::string filename = PathHelper::PathToUtf8(entry.path().filename());
                bool skipEntry = false;

                if (config.maxDepth != -1 && entry.is_directory() && currentDepth >= config.maxDepth) {
                    it.disable_recursion_pending();
                }
                if (config.maxDepth != -1 && currentDepth > config.maxDepth) {
                    ++it;
                    continue;
                }

                if (!skipEntry && entry.is_directory() && PathHelper::IsIgnoredDirectory(filename, config.includeIgnored)) {
                    it.disable_recursion_pending();
                    skipEntry = true;
                }

                if (!skipEntry && !config.includeHidden && PathHelper::IsHidden(entry)) {
                    if (entry.is_directory()) {
                        it.disable_recursion_pending();
                    }
                    skipEntry = true;
                }

                if (!skipEntry && config.minDepth != -1 && currentDepth < config.minDepth) {
                    skipEntry = true;
                }

                if (!skipEntry && !config.fileTypes.empty()) {
                    bool typeMatch = false;
                    for (char t : config.fileTypes) {
                        if (t == 'f' && entry.is_regular_file()) typeMatch = true;
                        if (t == 'd' && entry.is_directory()) typeMatch = true;
                        if (t == 'l' && entry.is_symlink()) typeMatch = true;
                        if (t == 'x' && PathHelper::IsExecutable(entry)) typeMatch = true;
                    }
                    if (!typeMatch) {
                        skipEntry = true;
                    }
                }

                if (!skipEntry && !config.extensions.empty()) {
                    std::string ext = entry.path().extension().string();
                    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                    ext = PathHelper::ToLower(ext);

                    bool extMatch = false;
                    for (const auto& targetExt : config.extensions) {
                        std::string normalizedTarget = PathHelper::ToLower(targetExt);
                        if (!normalizedTarget.empty() && normalizedTarget[0] == '.') {
                            normalizedTarget = normalizedTarget.substr(1);
                        }
                        if (normalizedTarget == ext) {
                            extMatch = true;
                            break;
                        }
                    }
                    if (!extMatch) {
                        skipEntry = true;
                    }
                }

                if (!skipEntry && CompiledPatternMatcher::Matches(filename, compiledPattern)) {
                    fs::path outPath = config.absolutePaths ? fs::absolute(entry.path()) : entry.path();
                    std::string displayStr = PathHelper::PathToUtf8(outPath);

                    if (!config.execCommand.empty()) {
                        std::string cmd = CommandExecutor::BuildExecCommand(config.execCommand, outPath);
                        if (!CommandExecutor::ExecuteCommandDirect(cmd)) {
                            std::cerr << Color::RED << "Error: failed to execute command: " << cmd << Color::RESET << "\n";
                        }
                    } else {
                        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
                            matches.push_back(displayStr);
                        } else if (config.print0) {
                            std::cout << displayStr << '\0';
                        } else {
                            std::cout << PathHelper::FormatPath(entry, displayStr, config.colorOutput) << "\n";
                        }
                    }
                }

                ++it;
            }
        } catch (const std::exception& e) {
            std::cerr << Color::RED << "Error during traversal: " << e.what() << Color::RESET << "\n";
        }
    }
};
