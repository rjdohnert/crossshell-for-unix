class CopyEngine {
private:
    CopyOptions options;
    ProgressBar progressBar;

    bool promptOverwrite(const fs::path& dest) {
        std::cout << "cp: overwrite \"" << dest.string() << "\"? (y/n [n]) ";
        std::string response;
        if (!std::getline(std::cin, response)) {
            return false;
        }
        return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
    }

    bool copySingleFile(const fs::path& src, const fs::path& dest) {
        std::error_code ec;

        if (fs::exists(dest, ec)) {
            if (options.interactive) {
                if (!promptOverwrite(dest)) {
                    std::cout << "cp: skipping \"" << dest.string() << "\"\n";
                    return true;
                }
            } else if (options.force) {
                fs::remove(dest, ec);
            }
        }

        uint64_t fileSize = fs::file_size(src, ec);
        if (ec) fileSize = 0;

        std::ifstream in(src, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "cp: cannot open source file: " << src.string() << "\n";
            return false;
        }

        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "cp: cannot create target file: " << dest.string() << "\n";
            return false;
        }

        if (options.showProgress) {
            progressBar.reset(fileSize);
        }

        constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB chunk
        std::vector<char> buffer(BUFFER_SIZE);

        while (in) {
            in.read(buffer.data(), buffer.size());
            std::streamsize bytesRead = in.gcount();
            if (bytesRead > 0) {
                out.write(buffer.data(), bytesRead);
                if (options.showProgress) {
                    progressBar.update(static_cast<uint64_t>(bytesRead), src.filename().string());
                }
            }
        }

        if (options.showProgress) {
            progressBar.update(0, src.filename().string(), true);
            progressBar.finish();
        }

        in.close();
        out.close();

        if (options.preserve || options.hpuxExtentAcl) {
            FileAttributePreserver::preserveAttributes(src, dest, options.preserve, options.hpuxExtentAcl);
        }

        if (options.outputJson) {
            std::cout << "{\"action\":\"copy\",\"source\":\"" << JsonSerializer::escape(src.string()) 
                      << "\",\"destination\":\"" << JsonSerializer::escape(dest.string()) 
                      << "\",\"bytes\":" << fileSize << "}\n";
        }

        return true;
    }

    bool copyDirectoryRecursive(const fs::path& src, const fs::path& dest) {
        std::error_code ec;
        fs::create_directories(dest, ec);

        for (const auto& entry : fs::directory_iterator(src, fs::directory_options::skip_permission_denied)) {
            const fs::path& currentPath = entry.path();
            fs::path targetPath = dest / currentPath.filename();

            if (entry.is_symlink()) {
                if (options.noDereference) {
                    fs::copy_symlink(currentPath, targetPath, ec);
                    continue;
                }
            }

            if (entry.is_directory()) {
                if (!copyDirectoryRecursive(currentPath, targetPath)) {
                    return false;
                }
            } else if (entry.is_regular_file()) {
                if (!copySingleFile(currentPath, targetPath)) {
                    return false;
                }
            }
        }
        return true;
    }

public:
    explicit CopyEngine(const CopyOptions& opts) : options(opts) {}

    bool execute() {
        if (options.sources.empty()) {
            std::cerr << "cp: missing file operand\nTry 'cp --help' for more information.\n";
            return false;
        }

        bool destIsDir = fs::is_directory(options.destination);

        if (options.sources.size() > 1 && !destIsDir) {
            std::cerr << "cp: target \"" << options.destination.string() 
                      << "\" is not a directory\n";
            return false;
        }

        for (const auto& src : options.sources) {
            if (!fs::exists(src)) {
                std::cerr << "cp: cannot stat \"" << src.string() << "\": No such file or directory\n";
                return false;
            }

            fs::path targetPath = destIsDir ? (options.destination / src.filename()) : options.destination;

            if (fs::is_directory(src)) {
                if (!options.recursive) {
                    std::cerr << "cp: omitting directory \"" << src.string() << "\" (use -r or -R)\n";
                    continue;
                }
                if (!copyDirectoryRecursive(src, targetPath)) {
                    return false;
                }
            } else {
                if (!copySingleFile(src, targetPath)) {
                    return false;
                }
            }
        }
        return true;
    }
};

// ============================================================================
// Class: HelpFormatter
// HP-UX formatted Reference Manual page.
// ============================================================================
