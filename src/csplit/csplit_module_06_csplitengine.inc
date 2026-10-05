class CsplitEngine {
private:
    CsplitOptions options;

public:
    explicit CsplitEngine(CsplitOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::string> lines;
        if (options.inputFile == "-") {
            std::string line;
            while (std::getline(std::cin, line)) {
                lines.push_back(line);
            }
        } else {
            std::ifstream in(options.inputFile);
            if (!in.is_open()) {
                std::cerr << "csplit: cannot open '" << options.inputFile << "'\n";
                return 1;
            }
            std::string line;
            while (std::getline(in, line)) {
                lines.push_back(line);
            }
        }

        std::vector<std::string> createdFiles;
        std::vector<std::pair<std::string, size_t>> pieces;
        int fileIndex = 0;
        size_t currentLine = 0;

        auto writePiece = [&](size_t start, size_t end, bool skip) -> bool {
            if (skip) return true;
            std::string outName = FileSplitManager::formatFilename(options.prefix, fileIndex++, options.digits, options.suffixFormat);
            std::ofstream out(outName, std::ios::binary);
            if (!out.is_open()) return false;

            size_t totalBytes = 0;
            for (size_t i = start; i < end && i < lines.size(); ++i) {
                out << lines[i] << "\n";
                totalBytes += lines[i].size() + 1;
            }
            out.close();

            createdFiles.push_back(outName);
            pieces.emplace_back(outName, totalBytes);
            if (!options.quiet && options.outputFormat == 0 && options.pipeCommand.empty()) {
                std::cout << totalBytes << "\n";
            }
            return true;
        };

        for (const auto& pat : options.patterns) {
            int iterations = (pat.repeat == -1) ? INT_MAX : (pat.repeat + 1);

            for (int rep = 0; rep < iterations && currentLine < lines.size(); ++rep) {
                size_t splitAt = lines.size();

                if (pat.type == PatternType::LineNum) {
                    if (pat.lineNum > 0 && static_cast<size_t>(pat.lineNum - 1) > currentLine) {
                        splitAt = static_cast<size_t>(pat.lineNum - 1);
                    } else {
                        break;
                    }
                } else {
                    std::regex re(pat.regexStr);
                    for (size_t i = currentLine; i < lines.size(); ++i) {
                        if (std::regex_search(lines[i], re)) {
                            long long target = static_cast<long long>(i) + pat.offset;
                            if (target >= static_cast<long long>(currentLine) && target <= static_cast<long long>(lines.size())) {
                                splitAt = static_cast<size_t>(target);
                                break;
                            }
                        }
                    }
                    if (splitAt == lines.size() && rep > 0) {
                        break; // {N} / {*} finished
                    }
                }

                if (splitAt > currentLine) {
                    if (!writePiece(currentLine, splitAt, pat.type == PatternType::RegexSkip)) {
                        if (!options.keepFiles) FileSplitManager::cleanup(createdFiles);
                        return 1;
                    }
                    currentLine = splitAt;
                }
            }
        }

        if (currentLine < lines.size()) {
            if (!writePiece(currentLine, lines.size(), false)) {
                if (!options.keepFiles) FileSplitManager::cleanup(createdFiles);
                return 1;
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            CsplitReporter::dispatch(pieces, options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

