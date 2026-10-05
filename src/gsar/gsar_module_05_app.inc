// ============================================================================
// Main Application Controller
// ============================================================================
class GSARApp {
private:
    Options options;

    void processFile(const std::string& inFilePath, const std::string& outFilePath) {
        fs::path inputPath(inFilePath);
        if (!fs::exists(inputPath)) {
            std::cerr << "[gsar] Error: Input file does not exist: " << inFilePath << "\n";
            return;
        }

        bool inPlace = outFilePath.empty() || (fs::absolute(inputPath) == fs::absolute(fs::path(outFilePath)));

        if (options.verbose) {
            std::cout << "[gsar] Processing file: " << inFilePath << (inPlace ? " (in-place)" : " -> " + outFilePath) << "\n";
        }

        std::ifstream inFile(inFilePath, std::ios::binary);
        if (!inFile) {
            std::cerr << "[gsar] Error: Unable to open input file: " << inFilePath << "\n";
            return;
        }

        SearchEngine engine(options);
        SearchEngine::Result res;

        if (options.countOnly || (!options.hasReplace && outFilePath.empty())) {
            // Read-only inspection
            res = engine.processStream(inFile, nullptr);
        } else if (inPlace) {
            // In-place replacement via temporary file
            fs::path tempPath = inputPath.parent_path() / (inputPath.filename().string() + ".gsar_tmp");
            {
                std::ofstream tempOut(tempPath, std::ios::binary);
                if (!tempOut) {
                    std::cerr << "[gsar] Error: Cannot create temporary file: " << tempPath.string() << "\n";
                    return;
                }
                res = engine.processStream(inFile, &tempOut);
            }
            inFile.close();

            if (options.createBackup) {
                fs::path backupPath = inputPath.string() + options.backupExtension;
                std::error_code ec;
                fs::copy_file(inputPath, backupPath, fs::copy_options::overwrite_existing, ec);
                if (ec && options.verbose) {
                    std::cerr << "[gsar] Warning: Failed to create backup file: " << ec.message() << "\n";
                }
            }

            std::error_code ec;
            fs::rename(tempPath, inputPath, ec);
            if (ec) {
                // If rename failed (e.g. across drives), fallback to copy & remove
                fs::copy_file(tempPath, inputPath, fs::copy_options::overwrite_existing, ec);
                fs::remove(tempPath, ec);
            }
        } else {
            // Different output file
            std::ofstream outFile(outFilePath, std::ios::binary);
            if (!outFile) {
                std::cerr << "[gsar] Error: Unable to open output file: " << outFilePath << "\n";
                return;
            }
            res = engine.processStream(inFile, &outFile);
        }

        if (options.countOnly || options.verbose) {
            std::cout << inFilePath << ": " << res.matchCount << " occurrence(s) found.\n";
        }
    }

public:
    explicit GSARApp(Options opt) : options(std::move(opt)) {}

    int run() {
        SET_BINARY_MODE(stdin);
        SET_BINARY_MODE(stdout);

        // Pipe / Filter mode
        if (options.filterPipeMode || (options.inputFiles.empty() && options.outputFile.empty())) {
            SearchEngine engine(options);
            SearchEngine::Result res = engine.processStream(std::cin, &std::cout);
            if (options.countOnly) {
                std::cerr << "Matches: " << res.matchCount << "\n";
            }
            return 0;
        }

        // File processing mode
        if (!options.outputFile.empty() && options.inputFiles.size() == 1) {
            processFile(options.inputFiles[0], options.outputFile);
        } else {
            for (const auto& file : options.inputFiles) {
                processFile(file, "");
            }
        }

        return 0;
    }
};

